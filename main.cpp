#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <errno.h>
#include <getopt.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <gattlib.h>
#include <vector>
#include <csignal>

#include "guitar.h"

// Reference code taken from:
// https://github.com/joprietoe/gdbus/blob/master/gdbus-example-server.c
// https://github.com/joprietoe/gdbus/blob/master/gdbus-example-client.c

// Ubuntu Distrobox Test:
// cd .. && rm -rf build && mkdir build && cd build && cmake .. && make && cpack ..

// The human-readable introspection data for the service we are exporting
static const gchar g_introspection_xml[] =
"<node>"
"  <interface name='com.blackseraph.ghlble'>"
"    <method name='StartScan'/>"
"    <method name='StopScan'/>"
"    <method name='GetScanStatus'>"
"      <arg type='b' name='status' direction='out'/>"
"    </method>"
"    <method name='GetConnectedDevices'>"
"      <arg type='as' name='mac_addresses' direction='out'/>"
"    </method>"
"  </interface>"
"</node>";

// The encoded introspection data for the service we are exporting
static GDBusNodeInfo *g_introspection_data;

// The process' main loop
static GMainLoop * g_main_loop;

// The control object registration ID
static guint g_control_object_registration_id;

// The Bluetooth scanning state
static gboolean g_is_scanning;

// The Bluetooth adapter
static gattlib_adapter_t* g_adapter;

// The Bluetooth scanning thread
std::unique_ptr<std::thread> g_bluetooth_scanning_thread;

// The connected guitars
static std::vector<std::unique_ptr<Guitar>> g_guitars;

// The invoke result
static int g_invoke_result;

// The Bluetooth device discovery callback
static void ble_discovered_device(gattlib_adapter_t* adapter, const char* addr, const char* name, void *user_data)
{
    // We've discovered a new guitar
    if (name != NULL && addr != NULL && strcmp(name, "Ble Guitar") == 0)
    {
        // Iterate the cached guitars
        for (const auto& guitar : g_guitars)
        {
            // We've found an already known guitar
            if (guitar->getAddress() == addr)
            {
                // Log the known guitar
                g_print("Ignoring the already known Guitar (%s).", addr);

                // Ignore known guitars
                return;
            }
        }

        // Guitar doesn't exist yet, create and add it
        g_guitars.push_back(std::make_unique<Guitar>(adapter, addr));
    }
}

// The Bluetooth scanning thread
static void bluetooth_scanning_thread()
{
    // We've got an open adapter
    if (g_adapter != NULL)
    {
        // Set the scanning state
        g_is_scanning = TRUE;

        // Log the call
        g_print("Starting scan\n");

        // Scan for Bluetooth devices
        gattlib_adapter_scan_enable(g_adapter, ble_discovered_device, 0, NULL);

        // Log the call
        g_print("Scan has ended\n");

        // Stop scanning
        gattlib_adapter_scan_disable(g_adapter);

        // Set the scanning state
        g_is_scanning = FALSE;
    }
}

// Executes methods
static void handle_method_call(GDBusConnection *connection, const gchar *sender, const gchar *object_path, const gchar *interface_name, const gchar *method_name, GVariant *parameters, GDBusMethodInvocation *invocation, gpointer user_data)
{
    if (g_strcmp0(method_name, "StartScan") == 0)
    {
        // We've got an open adapter and aren't scanning yet
        if (g_adapter != NULL && !g_is_scanning)
        {
            // We've got a live Bluetooth scanning thread
            if (g_bluetooth_scanning_thread != NULL && g_bluetooth_scanning_thread->joinable())
            {
                // Wait for the thread to finish
                g_bluetooth_scanning_thread->join();
            }

            // Create the Bluetooth scanning thread
            g_bluetooth_scanning_thread = std::make_unique<std::thread>(bluetooth_scanning_thread);
        }

        // Let the caller know his request was received
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
    else if (g_strcmp0(method_name, "StopScan") == 0)
    {
        // We've got an open adapter and are currently scanning
        if (g_adapter != NULL && g_is_scanning)
        {
            // Stop scanning
            gattlib_adapter_scan_disable(g_adapter);

            // We've got a live Bluetooth scanning thread
            if (g_bluetooth_scanning_thread != NULL && g_bluetooth_scanning_thread->joinable())
            {
                // Wait for the thread to finish
                g_bluetooth_scanning_thread->join();
            }

            // Log the call
            g_print("Disabled scanning\n");
        }

        // Let the caller know his request was received
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
    else if (g_strcmp0(method_name, "GetScanStatus") == 0)
    {
        // Return the scanning state
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", g_is_scanning));
    }
    else if (g_strcmp0(method_name, "GetConnectedDevices") == 0)
    {
        // Return the connected guitars
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("(as)"));
        g_variant_builder_open(&builder, G_VARIANT_TYPE("as"));
        for (const auto& guitar : g_guitars)
        {
            if (guitar->isConnected())
            {
                g_variant_builder_add(&builder, "s", guitar->getAddress().c_str());
            }
        }
        g_variant_builder_close(&builder);
        g_dbus_method_invocation_return_value(invocation, g_variant_builder_end(&builder));
    }
}

// Gets object properties
static GVariant * handle_get_property(GDBusConnection *connection, const gchar *sender, const gchar *object_path, const gchar *interface_name, const gchar *property_name, GError **error, gpointer user_data)
{
    // The property
    GVariant *ret = NULL;

    // The caller wants to query the current scan status
    if (g_strcmp0(property_name, "ScanStatus") == 0)
    {
        // Return the current scan status
        ret = g_variant_new_boolean(g_is_scanning);
    }

    // Return the property
    return ret;
}

// Sets object properties
static gboolean handle_set_property(GDBusConnection *connection, const gchar *sender, const gchar *object_path, const gchar *interface_name, const gchar *property_name, GVariant *value, GError **error, gpointer user_data)
{
    // We don't support setting properties yet
    return TRUE;
}

// The DBus interface's virtual function table
static const GDBusInterfaceVTable interface_vtable =
{
    handle_method_call,
    handle_get_property,
    handle_set_property
};

// Quits the main loop
static void quitMainLoop()
{
    // We've got an open adapter
    if (g_adapter != NULL)
    {
        // Stop scanning
        gattlib_adapter_scan_disable(g_adapter);
    }

    // We've got a live Bluetooth scanning thread
    if (g_bluetooth_scanning_thread != NULL && g_bluetooth_scanning_thread->joinable())
    {
        // Wait for the thread to finish
        g_bluetooth_scanning_thread->join();
    }

    // Disconnect all guitars
    g_guitars.clear();

    // We've got a running main loop
    if (g_main_loop != NULL)
    {
        // Stop the main loop
        g_main_loop_quit(g_main_loop);
    }
}

// The bus acquired callback
static void on_bus_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    // Register the control object
    g_control_object_registration_id = g_dbus_connection_register_object(connection, "/com/blackseraph/ghlble/control", g_introspection_data->interfaces[0], &interface_vtable, NULL /* user_data */, NULL /* user_data_free_func */, NULL /* GError** */);

    // We managed to register the control object
    if (g_control_object_registration_id > 0)
    {
        // Log the event
        g_print("Registered the control object\n");
    }

    // We failed to register the control object
    else
    {
        // Log the event
        g_print("Failed to register the control object\n");

        // Quit the main loop
        quitMainLoop();
    }
}

static void on_name_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    // Log the event
    g_print("Aquired the name\n");

    // Create the Bluetooth scanning thread
    g_bluetooth_scanning_thread = std::make_unique<std::thread>(bluetooth_scanning_thread);
}

static void on_name_lost(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    // Log the event
    g_print("Lost the name\n");

    // Quit the main loop
    quitMainLoop();
}

static void toggle_scanning(GDBusConnection *connection, const gchar *name, const gchar *name_owner, gpointer user_data)
{
    // Check if pairing should be enabled or disabled
    bool enabled = (bool)user_data;

    // Prepare the method name based on the desired state
    const gchar* method_name = enabled ? "StartScan" : "StopScan";

    // Invoke the method on the daemon
    GError* error = NULL;
    GVariant* result = g_dbus_connection_call_sync(
        connection,
        "com.blackseraph.ghlble",           // Name of the service
        "/com/blackseraph/ghlble/control",  // Object path
        "com.blackseraph.ghlble",           // Interface name
        method_name,                        // Method name
        NULL,                               // Parameters
        NULL,                               // Expected reply type
        G_DBUS_CALL_FLAGS_NONE,
        -1,                                 // Timeout (default)
        NULL,                               // GCancellable
        &error                              // GError
    );

    // Check for errors
    if (error != NULL)
    {
        g_printerr("Failed to invoke %s: %s\n", method_name, error->message);
        g_error_free(error);
        g_invoke_result = 1;  // Indicate failure
    }
    else
    {
        g_print("Successfully invoked %s\n", method_name);
        g_variant_unref(result);
        g_invoke_result = 0;  // Indicate success
    }

    // Quit the main loop
    quitMainLoop();
}

static void get_scan_status(GDBusConnection *connection, const gchar *name, const gchar *name_owner, gpointer user_data)
{
    // Invoke the method on the daemon
    GError* error = NULL;
    GVariant* result = g_dbus_connection_call_sync(
        connection,
        "com.blackseraph.ghlble",           // Name of the service
        "/com/blackseraph/ghlble/control",  // Object path
        "com.blackseraph.ghlble",           // Interface name
        "GetScanStatus",                    // Method name
        NULL,                               // Parameters
        G_VARIANT_TYPE("(b)"),              // Expected return type (boolean)
        G_DBUS_CALL_FLAGS_NONE,
        -1,                                 // Timeout (default)
        NULL,                               // GCancellable
        &error                              // GError
    );

    // Check for errors
    if (error != NULL)
    {
        g_printerr("Error calling GetScanStatus: %s\n", error->message);
        g_error_free(error);
        g_invoke_result = 1;  // Indicate failure
    }
    else
    {
        gboolean is_scanning;
        g_variant_get(result, "(b)", &is_scanning); // Extract the boolean value
        g_print("Scan status: %s\n", is_scanning ? "Scanning" : "Not scanning");
        g_variant_unref(result);
        g_invoke_result = 0;  // Indicate success
    }

    // Quit the main loop
    quitMainLoop();
}

static void get_connected_devices(GDBusConnection *connection, const gchar *name, const gchar *name_owner, gpointer user_data)
{
    // Invoke the method on the daemon
    GError* error = NULL;
    GVariant* result = g_dbus_connection_call_sync(
        connection,
        "com.blackseraph.ghlble",            // Name of the service
        "/com/blackseraph/ghlble/control",  // Object path
        "com.blackseraph.ghlble",           // Interface name
        "GetConnectedDevices",              // Method name
        NULL,                               // Parameters
        G_VARIANT_TYPE("(as)"),             // Expected return type (array of strings)
        G_DBUS_CALL_FLAGS_NONE,
        -1,                                 // Timeout (default)
        NULL,                               // GCancellable
        &error                              // GError
    );

    // Check for errors
    if (error != NULL)
    {
        g_printerr("Error calling GetConnectedDevices: %s\n", error->message);
        g_error_free(error);
        g_invoke_result = 1;  // Indicate failure
    }
    else
    {
        GVariant *array = g_variant_get_child_value(result, 0);
        gchar *mac_address;
        GVariantIter iter;
        g_variant_iter_init(&iter, array);
        while (g_variant_iter_next(&iter, "s", &mac_address))
        {
            g_print("%s\n", mac_address);
            g_free(mac_address);  // Free each string after use
        }
        g_variant_unref(array);
        g_variant_unref(result);
        g_invoke_result = 0;  // Indicate success
    }

    // Quit the main loop
    quitMainLoop();
}

static void on_name_vanished(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    // Print the error
    g_printerr("The daemon isn't running\n");

    // Set the invoke result
    g_invoke_result = 1;

    // Quit the main loop
    quitMainLoop();
}

// The signal handler function
static void handleSignal(int signal)
{
    // We're being asked to stop
    if (signal == SIGTERM || signal == SIGINT)
    {
        // Quit the main loop
        quitMainLoop();
    }
}

// Prints the usage
void print_usage()
{
    // Print the usage
    g_print(
        "Usage:\n"
        "\t--daemon\tRuns the Guitar Hero Live daemon\n"
        "\t--scan=[on|off]\tToggles guitar scanning on or off or reads the current setting\n"
        "\t--guitars\tShows connected guitars\n"
    );
}

// Executes the given callback upon the appearance of the name
int execute_with_callbacks(GBusNameAppearedCallback appeared_callback, gpointer user_data)
{
    // Install signal handlers
    std::signal(SIGTERM, handleSignal);
    std::signal(SIGINT, handleSignal);

    // Watch the name
    guint watcher_id = g_bus_watch_name(
        G_BUS_TYPE_SESSION,
        "com.blackseraph.ghlble",
        G_BUS_NAME_WATCHER_FLAGS_NONE,
        appeared_callback,
        on_name_vanished,
        user_data,
        NULL);

    // Create a main loop
    g_main_loop = g_main_loop_new(NULL, FALSE);

    // Run the main loop
    g_main_loop_run(g_main_loop);

    // Unwatch the name
    g_bus_unwatch_name(watcher_id);

    // Return the result
    return g_invoke_result;
}

// Runs the daemon
int run_daemon()
{
    // Install signal handlers
    std::signal(SIGTERM, handleSignal);
    std::signal(SIGINT, handleSignal);

    // Open the Bluetooth adapter
    int result = gattlib_adapter_open(NULL, &g_adapter);

    // We managed to open the Bluetooth adapter
    if (result == GATTLIB_SUCCESS)
    {
        // Log the event
        g_print("Opened the Bluetooth adapter\n");

        // Parse the DBus introspection data
        g_introspection_data = g_dbus_node_info_new_for_xml(g_introspection_xml, NULL);

        // We managed to parse the DBus introspection data
        if (g_introspection_data != NULL)
        {
            // Log the event
            g_print("Parsed the DBus introspection data\n");

            // Own the name
            guint owner_id = g_bus_own_name(G_BUS_TYPE_SESSION, "com.blackseraph.ghlble", G_BUS_NAME_OWNER_FLAGS_NONE, on_bus_acquired, on_name_acquired, on_name_lost, NULL, NULL);

            // Create a main loop
            g_main_loop = g_main_loop_new(NULL, FALSE);

            // Run the main loop
            g_main_loop_run(g_main_loop);

            // Unown the name
            g_bus_unown_name(owner_id);

            // Free the introspection data
            g_dbus_node_info_unref(g_introspection_data);
        }

        // We failed to parse the DBus introspection data (which means we ran out of memory)
        else
        {
            // Log the event
            g_print("Failed to parse the DBus introspection data\n");

            // Set the result
            result = ENOMEM;
        }

        // Close the adapter
        gattlib_adapter_close(g_adapter);

        // Log the event
        g_print("Closed the Bluetooth adapter\n");
    }

    // We failed to open the Bluetooth adapter
    else
    {
        // Log the event
        g_print("Failed to open the Bluetooth adapter\n");
    }

    // Return the result
    return result;
}

// The entry point
int main(int argc, char *argv[])
{
    // The result
    int result = 1;
    
    // Define long options
    static struct option long_options[] = {
        {"daemon", no_argument, nullptr, 'd'},
        {"scan", optional_argument, nullptr, 's'},
        {"guitars", optional_argument, nullptr, 'g'},
        {nullptr, 0, nullptr, 0}
    };

    // Parse options
    int opt = -1;
    int option_index = -1;
    while ((opt = getopt_long(argc, argv, "d:s:g", long_options, &option_index)) != -1)
    {
        switch (opt)
        {
            case 'd':
                result = run_daemon();
                break;
            case 's':
                if (optarg != NULL)
                {
                    result = execute_with_callbacks(toggle_scanning, (gpointer)(std::string(optarg) == "on"));
                }
                else
                {
                    result = execute_with_callbacks(get_scan_status, NULL);
                }
                break;
            case 'g':
                result = execute_with_callbacks(get_connected_devices, NULL);
                break;
            case '?':
            default:
                print_usage();
        }
    }

    // We haven't been provided options
    if (opt == -1 && option_index == -1)
    {
        print_usage();
    }

    // Return the result
    return result;
}