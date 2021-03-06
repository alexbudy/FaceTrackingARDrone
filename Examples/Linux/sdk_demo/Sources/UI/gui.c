#include <stdlib.h>
#include "gui.h"
#include <ardrone_tool/UI/ardrone_input.h>
 
gui_t *gui = NULL;
 
gui_t *get_gui()
{
  return gui;
}
 
/* If the drone is landed, only start is clickable,
   if the drone is in the air, only stop is clickable
*/
static void toggleButtonsState(void)
{
  gboolean start_state = gtk_widget_get_sensitive(gui->start);
 
  gtk_widget_set_sensitive(gui->start, !start_state);
  gtk_widget_set_sensitive(gui->stop, start_state);
}
 
static void buttons_callback( GtkWidget *widget,
                  gpointer   data )
{
  static int value = 1;
  ardrone_tool_set_ui_pad_start(value);
  if (value)
    g_print("Taking off");
  else
    g_print("Landing");
  value = (value + 1) % 2;
  toggleButtonsState(); // We want only one button to be clickable
}
 
static void on_destroy(GtkWidget *widget, gpointer data)
{
  vp_os_free(gui);
  gtk_main_quit();
}
 
void init_gui(int argc, char **argv)
{
  gui = vp_os_malloc(sizeof (gui_t));
 
  g_thread_init(NULL);
  gdk_threads_init();
  gtk_init(&argc, &argv);
 
  gui->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  g_signal_connect(G_OBJECT(gui->window),
           "destroy",
           G_CALLBACK(on_destroy),
           NULL);
  gui->box = gtk_vbox_new(FALSE, 10);
  gtk_container_add(GTK_CONTAINER(gui->window),
            gui->box);
  gui->cam = gtk_image_new();
  gtk_box_pack_start(GTK_BOX(gui->box), gui->cam, FALSE, TRUE, 0);
 
  gui->start = gtk_button_new_with_label("Start");
  g_signal_connect (gui->start, "clicked",
              G_CALLBACK (buttons_callback), NULL);
  gui->stop = gtk_button_new_with_label("Stop");
  g_signal_connect (gui->stop, "clicked",
              G_CALLBACK (buttons_callback), NULL);
    gtk_widget_set_sensitive(gui->start, TRUE);
  gtk_widget_set_sensitive(gui->stop, FALSE);
 
  gtk_box_pack_start(GTK_BOX(gui->box), gui->start, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(gui->box), gui->stop, TRUE, TRUE, 0);
 
  gtk_widget_show_all(gui->window);
}

