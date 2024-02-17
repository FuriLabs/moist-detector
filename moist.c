#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <glib.h>
#include <dbus/dbus-glib.h>

#define EVENT_SIZE  (sizeof (struct inotify_event))
#define BUF_LEN     (1024 * (EVENT_SIZE + 16))

int send_notification() {
  GError *error = NULL;
  DBusGConnection *connection = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
  if (connection == NULL) {
    g_printerr("Failed to open connection to bus: %s\n", error->message);
    g_error_free(error);
    return 1;
  }

  DBusGProxy *proxy = dbus_g_proxy_new_for_name (connection,
      "org.freedesktop.Notifications",
      "/org/freedesktop/Notifications",
      "org.freedesktop.Notifications"
  );

  GHashTable *hash = g_hash_table_new (g_str_hash, g_str_equal);

  dbus_g_proxy_call_no_reply(proxy, "Notify",
          G_TYPE_STRING, "System Warning",
          G_TYPE_UINT, 0,
          G_TYPE_STRING, "whatever_icon",
          G_TYPE_STRING, "Charging Disabled",
          G_TYPE_STRING, "Moisture detected in the charging port",
          G_TYPE_STRV, NULL,
          dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE), hash,
          G_TYPE_INT, -1,
          G_TYPE_INVALID
  );

  g_hash_table_destroy (hash);
  g_object_unref(proxy);

  return 0;
}

int main(int argc, char **argv) {
  int length;
  int fd;
  int wd;
  char buffer[BUF_LEN];
  FILE* file_pointer;

  fd = inotify_init();

  if (fd < 0) {
    perror("inotify_init");
  }

  wd = inotify_add_watch(fd, "/sys/devices/virtual/sec/ccic/", IN_MODIFY);

  while (1) {
    int i = 0;
    length = read(fd, buffer, BUF_LEN);

    if (length < 0) {
      perror("read");
    }

    while (i < length) {
      struct inotify_event *event = (struct inotify_event *) &buffer[i];

      if (event->len) {
        if (event->mask & IN_MODIFY) {
          if (!(event->mask & IN_ISDIR)) {
            if (strstr(event->name, "water") != NULL) {
//              printf("The file %s was modified.\n", event->name);

              file_pointer = fopen("/sys/devices/virtual/sec/ccic/water", "r");
              while(fgets(buffer, BUF_LEN, file_pointer)) {
                  if (buffer[0] == '1') {
//                    printf("%c\n", buffer[0]);
                    send_notification();
                  } else if (buffer[0] == '0') {
//                    printf("%c\n", buffer[0]);
                  }

                  break;
              }

              fclose(file_pointer);
            }
          }
        }
      }

      i += EVENT_SIZE + event->len;
    }
  }

  (void) inotify_rm_watch(fd, wd);
  (void) close(fd);

  return 0;
}
