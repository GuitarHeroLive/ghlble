#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <errno.h>
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

// The human-readable introspection data for the service we are exporting
static const gchar g_introspection_xml[] =
"<node>"
"  <interface name='com.blackseraph.ghlble.control'>"
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
static std::thread g_bluetooth_scanning_thread;

// The connected guitars
static std::vector<std::unique_ptr<Guitar>> g_guitars;

// The Bluetooth device discovery callback
static void ble_discovered_device(gattlib_adapter_t* adapter, const char* addr, const char* name, void *user_data)
{
    // We've discovered a new guitar
    if (name != NULL && addr != NULL && strcmp(name, "Ble Guitar") == 0)
    {
        // Create a new object for this guitar
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

        // Scan for Bluetooth devices
        gattlib_adapter_scan_enable(g_adapter, ble_discovered_device, 0, NULL);

        // Set the scanning state
        g_is_scanning = FALSE;
    }
}

// Executes methods
static void handle_method_call(GDBusConnection *connection, const gchar *sender, const gchar *object_path, const gchar *interface_name, const gchar *method_name, GVariant *parameters, GDBusMethodInvocation *invocation, gpointer user_data)
{
    if (g_strcmp0 (method_name, "StartScan") == 0)
    {
        // We've got an open adapter and aren't scanning yet
        if (g_adapter != NULL && !g_is_scanning)
        {
            // Create the Bluetooth scanning thread
            g_bluetooth_scanning_thread = std::thread(bluetooth_scanning_thread);
        }

        // Let the caller know his request was received
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
    else if (g_strcmp0 (method_name, "StopScan") == 0)
    {
        // We've got an open adapter and are currently scanning
        if (g_adapter != NULL && g_is_scanning)
        {
            // Stop scanning
            gattlib_adapter_scan_disable(g_adapter);
        }

        // Let the caller know his request was received
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
    else if (g_strcmp0 (method_name, "GetScanStatus") == 0)
    {
        // Return the scanning state
        g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", g_is_scanning));
    }
    else if (g_strcmp0 (method_name, "GetConnectedDevices") == 0)
    {
        // Allocate a guitar MAC address string array
        gchar **mac_addresses = (gchar **) g_malloc0(sizeof(gchar *) * g_guitars.size());

        // We managed to allocate a guitar MAC address string array
        if (mac_addresses != NULL)
        {
            // The guitar index
            guint i = 0;

            // Iterate the connected guitars
            for (const auto& guitar : g_guitars)
            {
                // Get the guitar's MAC address
                mac_addresses[i++] = g_strdup(guitar->getAddress().c_str());
            }

            // Return the guitar MAC addresses
            g_dbus_method_invocation_return_value(invocation, g_variant_new ("(as)", mac_addresses, g_guitars.size()));

            // Iterate the duplicated guitar MAC addresses
            for (i = 0; i < g_guitars.size(); i++)
            {
                // Free the duplicated guitar MAC address
                g_free(mac_addresses[i]);
            }

            // Free the guitar MAC address string array
            g_free(mac_addresses);
        }
    }
}

// Gets object properties
static GVariant * handle_get_property(GDBusConnection *connection, const gchar *sender, const gchar *object_path, const gchar *interface_name, const gchar *property_name, GError **error, gpointer user_data)
{
    // The property
    GVariant *ret = NULL;

    // The caller wants to query the current scan status
    if (g_strcmp0 (property_name, "ScanStatus") == 0)
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
    if (g_bluetooth_scanning_thread.joinable())
    {
        // Wait for the thread to finish
        g_bluetooth_scanning_thread.join();
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

    // Set the scanning state
    g_is_scanning = TRUE;

    // Create the Bluetooth scanning thread
    g_bluetooth_scanning_thread = std::thread(bluetooth_scanning_thread);
}

static void on_name_lost(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    // Log the event
    g_print("Lost the name\n");

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

// The entry point
int main(int argc, char *argv[])
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
            guint owner_id = g_bus_own_name(G_BUS_TYPE_SESSION, "com.blackseraph.ghlble.control", G_BUS_NAME_OWNER_FLAGS_NONE, on_bus_acquired, on_name_acquired, on_name_lost, NULL, NULL);

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
