/* byzanz-gui-window.c
 *
 * Copyright 2018 Abdullahi Usman
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "byzanz-gui-config.h"
#include "byzanz-gui-window.h"


typedef enum RecordStatus { RECORDING, NOT_RECORDING } RecordStatus;

struct _ByzanzGuiWindow {
  GtkApplicationWindow parent_instance;

  /* Template widgets */
  GtkHeaderBar        *header_bar;

  /* private members */
  GtkButton           *cancel;
  GtkButton           *start_record;

  GtkRadioButton      *whole_screen;
  GtkRadioButton      *current_screen;

  GtkCheckButton      *audio;
  GtkCheckButton      *cursor;
  GtkCheckButton      *ctrl_alt_r;

  GtkSpinButton       *delay;
  GtkSpinButton       *duration;

  GtkEntry            *save_file;

  GtkBox              *content;

  GSubprocess         *process;

  RecordStatus record_status;
};

G_DEFINE_TYPE(ByzanzGuiWindow, byzanz_gui_window, GTK_TYPE_APPLICATION_WINDOW)

static void set_new_record_file_name(ByzanzGuiWindow *self)
{
  GString *home;
  GFile   *file;

  home = g_string_new(g_getenv("HOME"));

  file = g_file_new_for_path((char* )g_string_append(home, "/Videos")->str);

  if (!g_file_query_exists(file, NULL))
    {
      if (!g_file_make_directory_with_parents(file, NULL, NULL))
        return;
    }

  g_string_append(home, g_date_time_format(g_date_time_new_now_local(), "/%c.mp4"));
  gtk_entry_set_text(self->save_file, home->str);
}

static gboolean process_key_events(GtkWidget *button, GdkEvent *event, gpointer data)
{
  GdkEventKey *key_event;
  ByzanzGuiWindow *self;

  self = BYZANZ_GUI_WINDOW(data);

  key_event = ((GdkEventKey*)event);

  if (key_event->state & GDK_CONTROL_MASK && key_event->state & GDK_MOD1_MASK && (key_event->keyval == GDK_KEY_R || key_event->keyval == GDK_KEY_r))
    {
      g_subprocess_force_exit(self->process);
      return TRUE;
    }

  return FALSE;
}
static GdkWindow *get_root_window(ByzanzGuiWindow *self)
{
  GdkScreen *screen;
  GdkWindow *window;

  screen = gtk_window_get_screen(GTK_WINDOW(self));
  window = gdk_screen_get_root_window(screen);

  return window;
}

static GdkSeat* get_seat(ByzanzGuiWindow *self)
{
  GdkSeat *seat;

  seat = gdk_display_get_default_seat(gdk_window_get_display(get_root_window(self)));

  return seat;
}

static void grab_keyboard_events(ByzanzGuiWindow *self)
{
  GdkGrabStatus grab_status;
  GdkSeat *seat;

  seat = get_seat(self);

  grab_status = gdk_seat_grab(seat, gtk_widget_get_window(GTK_WIDGET(self)), GDK_SEAT_CAPABILITY_KEYBOARD, FALSE, NULL, NULL, NULL, NULL);

  if (grab_status == GDK_GRAB_SUCCESS)
    {
      g_signal_connect(self, "key-press-event", G_CALLBACK(process_key_events), self);
    }
}

static void ungrab_keyboard_events(ByzanzGuiWindow *self)
{
  gdk_seat_ungrab(get_seat(self));
}

void cancel_clicked_cb(GtkWidget *button,
                       gpointer data)
{
  ByzanzGuiWindow *self;

  self = BYZANZ_GUI_WINDOW(data);

  if (self->record_status == RECORDING)
    g_subprocess_force_exit(self->process);
  else
    g_application_quit(g_application_get_default());
}

void on_process_finish(GObject *source_object,
                       GAsyncResult *res,
                       gpointer user_data)
{
  ByzanzGuiWindow *self;
  GtkDialog *dialog;
  GtkResponseType status;
  GtkLabel *message;

  self = BYZANZ_GUI_WINDOW(user_data);

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(self->ctrl_alt_r)))
    ungrab_keyboard_events(self);

  message = GTK_LABEL (gtk_label_new(NULL));
  gtk_label_set_markup (message, g_markup_printf_escaped ("Recording finished successfully...\nWould you like to record again ?\n<a href=\"file://%s\">Open File</a>", gtk_entry_get_text (self->save_file)));

  self->record_status = NOT_RECORDING;
  gtk_widget_set_state_flags(GTK_WIDGET(self->start_record), GTK_STATE_FLAG_NORMAL, TRUE);
  gtk_widget_set_state_flags(GTK_WIDGET(self->content), GTK_STATE_FLAG_NORMAL, TRUE);

  dialog = GTK_DIALOG(gtk_dialog_new_with_buttons("Recording Finished", GTK_WINDOW(self), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, "No", GTK_RESPONSE_NO, "Yes", GTK_RESPONSE_YES, "Delete Recording", GTK_RESPONSE_APPLY, NULL));

  gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(dialog)), GTK_WIDGET (message));
  gtk_widget_show(GTK_WIDGET(message));

  status = gtk_dialog_run(dialog);

  if (status == GTK_RESPONSE_NO)
    {
      g_application_quit(g_application_get_default());
    }
  else
    {
      if (status == GTK_RESPONSE_APPLY)
        g_file_delete(g_file_new_for_path(gtk_entry_get_text(self->save_file)), NULL, NULL);

      set_new_record_file_name(self);
      gtk_window_deiconify(GTK_WINDOW(self));
    }

  gtk_widget_destroy(GTK_WIDGET(dialog));
}

static GdkRectangle get_active_window_area(ByzanzGuiWindow *self)
{
  GdkWindow *active_window;
  GList *windows;
  GdkRectangle rect = { -1, -1, -1, -1 };

  windows = gdk_screen_get_window_stack(gdk_window_get_screen(get_root_window(self)));

  while (TRUE)
    {
      if (windows->next == NULL || windows->next->next == NULL)
        break;

      windows = windows->next;
    }

  active_window = windows->data;

  if (active_window != NULL)
    {
      rect.width = gdk_window_get_width(active_window);
      rect.height = gdk_window_get_height(active_window);

      gdk_window_get_root_origin(active_window, &rect.x, &rect.y);
    }

  g_list_free(windows);
  g_object_unref(active_window);
  return rect;
}

void start_record_clicked_cb(GtkWidget *button,
                             gpointer data)
{
  ByzanzGuiWindow *self;
  const gchar *save_file_name;
  GError *error;
  GFile *save_path;
  GFileInfo *save_path_info;

  self = BYZANZ_GUI_WINDOW(data);
  error = NULL;

  save_file_name = gtk_entry_get_text(self->save_file);
  save_path = g_file_get_parent(g_file_new_for_path(save_file_name));
  g_file_make_directory_with_parents(save_path, NULL, NULL);
  save_path_info = g_file_query_info(save_path, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, &error);

  if (!save_path_info || !g_file_info_get_attribute_boolean(save_path_info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
    {
      GtkDialog *dialog;

      dialog = GTK_DIALOG(gtk_message_dialog_new(GTK_WINDOW(self), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, "Cannot write to output directory\nCheck and try again."));

      gtk_dialog_run(dialog);
      gtk_widget_destroy(GTK_WIDGET(dialog));

      if (error != NULL)
        g_log(NULL, G_LOG_LEVEL_WARNING, "pyzanz-gui: %s", error->message);
    }
  else
    {
      const gchar *prog_name, *verbose, *audio, *cursor, **args;
      gchar *delay, *duration, *x, *y, *width, *height;
      GArray *argument_builder;
      gboolean grab_current_screen;

      prog_name = "byzanz-record";
      verbose = "--verbose";
      error = NULL;
      cursor = audio = NULL;
      argument_builder = g_array_new(TRUE, TRUE, sizeof(char*));

      g_array_append_val(argument_builder, prog_name);
      g_array_append_val(argument_builder, verbose);

      if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(self->audio)))
        {
          audio = "--audio";
          g_array_append_val(argument_builder, audio);
        }

      if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(self->cursor)))
        {
          cursor = "--cursor";
          g_array_append_val(argument_builder, cursor);
        }

      gtk_spin_button_update(self->delay);
      gtk_spin_button_update(self->duration);

      delay = g_strdup_printf("--delay=%d", gtk_spin_button_get_value_as_int(self->delay));

      duration = g_strdup_printf("--duration=%d", gtk_spin_button_get_value_as_int(self->duration));

      g_array_append_val(argument_builder, delay);
      g_array_append_val(argument_builder, duration);

      grab_current_screen = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(self->current_screen));
      if (grab_current_screen)
        {
          GdkRectangle active_window;

          active_window = get_active_window_area(self);

          if (active_window.width > 0 && active_window.height > 0)
            {
              x = g_strdup_printf("--x=%d", active_window.x);
              y = g_strdup_printf("--y=%d", active_window.y);
              width = g_strdup_printf("--width=%d", active_window.width);
              height = g_strdup_printf("--height=%d", active_window.height);

              g_array_append_val(argument_builder, x);
              g_array_append_val(argument_builder, y);
              g_array_append_val(argument_builder, width);
              g_array_append_val(argument_builder, height);
            }
        }

      g_array_append_val(argument_builder, save_file_name);

      args = (const gchar**)g_malloc(sizeof(gchar *) * argument_builder->len + 1);

      for (int i = 0; i < argument_builder->len; i++)
        {
          args[i] = g_array_index(argument_builder, gchar *, i);

          if (i == argument_builder->len - 1)
            args[i + 1] = NULL;
        }

      self->process = g_subprocess_newv(args, G_SUBPROCESS_FLAGS_STDERR_MERGE, &error);


      if (error != NULL)
        {
          g_log(NULL, G_LOG_LEVEL_ERROR, "Error while launching process: %s", error->message);
        }
      else
        {
          self->record_status = RECORDING;
          gtk_widget_set_state_flags(GTK_WIDGET(self->start_record), GTK_STATE_FLAG_INSENSITIVE, TRUE);
          gtk_widget_set_state_flags(GTK_WIDGET(self->content), GTK_STATE_FLAG_INSENSITIVE, TRUE);
          g_subprocess_wait_async(self->process, NULL, on_process_finish, self);

          gtk_window_iconify(GTK_WINDOW(self));

          if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(self->ctrl_alt_r)))
            grab_keyboard_events(self);
        }

      if (grab_current_screen)
        {
          g_free(x);
          g_free(y);
          g_free(width);
          g_free(height);
        }

      g_free(args);
      g_free(delay);
      g_free(duration);
      g_array_unref(argument_builder);
    }

  g_object_unref(save_path);
  g_object_unref(save_path_info);
}

static void
byzanz_gui_window_class_init(ByzanzGuiWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

  gtk_widget_class_set_template_from_resource(widget_class, "/org/gnome/Byzanz-Gui/byzanz-gui-window.ui");
  gtk_widget_class_bind_template_child(widget_class, ByzanzGuiWindow, header_bar);
  gtk_widget_class_bind_template_child(widget_class, ByzanzGuiWindow, cancel);
  gtk_widget_class_bind_template_child(widget_class, ByzanzGuiWindow, whole_screen);
  gtk_widget_class_bind_template_child(widget_class, ByzanzGuiWindow, current_screen);
  gtk_widget_class_bind_template_child(widget_class, ByzanzGuiWindow, start_record);
  gtk_widget_class_bind_template_child(widget_class, ByzanzGuiWindow, audio);
  gtk_widget_class_bind_template_child(widget_class, ByzanzGuiWindow, cursor);
  gtk_widget_class_bind_template_child(widget_class, ByzanzGuiWindow, ctrl_alt_r);
  gtk_widget_class_bind_template_child(widget_class, ByzanzGuiWindow, delay);
  gtk_widget_class_bind_template_child(widget_class, ByzanzGuiWindow, duration);
  gtk_widget_class_bind_template_child(widget_class, ByzanzGuiWindow, save_file);
  gtk_widget_class_bind_template_child(widget_class, ByzanzGuiWindow, content);

  gtk_widget_class_bind_template_callback(widget_class, cancel_clicked_cb);
  gtk_widget_class_bind_template_callback(widget_class, start_record_clicked_cb);
}

static void
byzanz_gui_window_init(ByzanzGuiWindow *self)
{
  gtk_widget_init_template(GTK_WIDGET(self));

  self->record_status = NOT_RECORDING;

  set_new_record_file_name(self);
}
