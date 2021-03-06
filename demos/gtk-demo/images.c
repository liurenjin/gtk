/* Images
 *
 * GtkImage is used to display an image; the image can be in a number of formats.
 * Typically, you load an image into a GdkPixbuf, then display the pixbuf.
 *
 * This demo code shows some of the more obscure cases, in the simple
 * case a call to gtk_image_new_from_file() is all you need.
 *
 * If you want to put image data in your program as a C variable,
 * use the make-inline-pixbuf program that comes with GTK+.
 * This way you won't need to depend on loading external files, your
 * application binary can be self-contained.
 */

#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <errno.h>

static GtkWidget *window = NULL;
static GdkPixbufLoader *pixbuf_loader = NULL;
static guint load_timeout = 0;
static GInputStream * image_stream = NULL;

static void
progressive_prepared_callback (GdkPixbufLoader *loader,
                               gpointer         data)
{
  GdkPixbuf *pixbuf;
  GtkWidget *image;

  image = GTK_WIDGET (data);

  pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

  /* Avoid displaying random memory contents, since the pixbuf
   * isn't filled in yet.
   */
  gdk_pixbuf_fill (pixbuf, 0xaaaaaaff);

  gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);
}

static void
progressive_updated_callback (GdkPixbufLoader *loader,
                              gint                 x,
                              gint                 y,
                              gint                 width,
                              gint                 height,
                              gpointer     data)
{
  GtkWidget *image;
  GdkPixbuf *pixbuf;

  image = GTK_WIDGET (data);

  pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
  gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);
}

static gint
progressive_timeout (gpointer data)
{
  GtkWidget *image;

  image = GTK_WIDGET (data);

  /* This shows off fully-paranoid error handling, so looks scary.
   * You could factor out the error handling code into a nice separate
   * function to make things nicer.
   */

  if (image_stream)
    {
      gssize bytes_read;
      guchar buf[256];
      GError *error = NULL;

      bytes_read = g_input_stream_read (image_stream, buf, 256, NULL, &error);

      if (bytes_read < 0)
        {
          GtkWidget *dialog;

          dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                           GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           "Failure reading image file 'alphatest.png': %s",
                                           error->message);
          g_error_free (error);

          g_signal_connect (dialog, "response",
                            G_CALLBACK (gtk_widget_destroy), NULL);

          g_object_unref (image_stream);
          image_stream = NULL;

          gtk_widget_show (dialog);

          load_timeout = 0;

          return FALSE; /* uninstall the timeout */
        }

      if (!gdk_pixbuf_loader_write (pixbuf_loader,
                                    buf, bytes_read,
                                    &error))
        {
          GtkWidget *dialog;

          dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                           GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           "Failed to load image: %s",
                                           error->message);

          g_error_free (error);

          g_signal_connect (dialog, "response",
                            G_CALLBACK (gtk_widget_destroy), NULL);

          g_object_unref (image_stream);
          image_stream = NULL;

          gtk_widget_show (dialog);

          load_timeout = 0;

          return FALSE; /* uninstall the timeout */
        }

      if (bytes_read == 0)
        {
          /* Errors can happen on close, e.g. if the image
           * file was truncated we'll know on close that
           * it was incomplete.
           */
          error = NULL;
          if (!g_input_stream_close (image_stream, NULL, &error))
            {
              GtkWidget *dialog;

              dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_CLOSE,
                                               "Failed to load image: %s",
                                               error->message);

              g_error_free (error);

              g_signal_connect (dialog, "response",
                                G_CALLBACK (gtk_widget_destroy), NULL);

              gtk_widget_show (dialog);

              g_object_unref (image_stream);
              image_stream = NULL;
              g_object_unref (pixbuf_loader);
              pixbuf_loader = NULL;

              load_timeout = 0;

              return FALSE; /* uninstall the timeout */
            }

          g_object_unref (image_stream);
          image_stream = NULL;

          /* Errors can happen on close, e.g. if the image
           * file was truncated we'll know on close that
           * it was incomplete.
           */
          error = NULL;
          if (!gdk_pixbuf_loader_close (pixbuf_loader,
                                        &error))
            {
              GtkWidget *dialog;

              dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_CLOSE,
                                               "Failed to load image: %s",
                                               error->message);

              g_error_free (error);

              g_signal_connect (dialog, "response",
                                G_CALLBACK (gtk_widget_destroy), NULL);

              gtk_widget_show (dialog);

              g_object_unref (pixbuf_loader);
              pixbuf_loader = NULL;

              load_timeout = 0;

              return FALSE; /* uninstall the timeout */
            }

          g_object_unref (pixbuf_loader);
          pixbuf_loader = NULL;
        }
    }
  else
    {
      GError *error = NULL;

      image_stream = g_resources_open_stream ("/images/alphatest.png", 0, &error);

      if (image_stream == NULL)
        {
          GtkWidget *dialog;

          dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                           GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           "%s", error->message);
          g_error_free (error);

          g_signal_connect (dialog, "response",
                            G_CALLBACK (gtk_widget_destroy), NULL);

          gtk_widget_show (dialog);

          load_timeout = 0;

          return FALSE; /* uninstall the timeout */
        }

      if (pixbuf_loader)
        {
          gdk_pixbuf_loader_close (pixbuf_loader, NULL);
          g_object_unref (pixbuf_loader);
        }

      pixbuf_loader = gdk_pixbuf_loader_new ();

      g_signal_connect (pixbuf_loader, "area-prepared",
                        G_CALLBACK (progressive_prepared_callback), image);

      g_signal_connect (pixbuf_loader, "area-updated",
                        G_CALLBACK (progressive_updated_callback), image);
    }

  /* leave timeout installed */
  return TRUE;
}

static void
start_progressive_loading (GtkWidget *image)
{
  /* This is obviously totally contrived (we slow down loading
   * on purpose to show how incremental loading works).
   * The real purpose of incremental loading is the case where
   * you are reading data from a slow source such as the network.
   * The timeout simply simulates a slow data source by inserting
   * pauses in the reading process.
   */
  load_timeout = g_timeout_add (150, progressive_timeout, image);
  g_source_set_name_by_id (load_timeout, "[gtk+] progressive_timeout");
}

static void
cleanup_callback (GObject   *object,
                  gpointer   data)
{
  if (load_timeout)
    {
      g_source_remove (load_timeout);
      load_timeout = 0;
    }

  if (pixbuf_loader)
    {
      gdk_pixbuf_loader_close (pixbuf_loader, NULL);
      g_object_unref (pixbuf_loader);
      pixbuf_loader = NULL;
    }

  if (image_stream)
    {
      g_object_unref (image_stream);
      image_stream = NULL;
    }
}

static void
toggle_sensitivity_callback (GtkWidget *togglebutton,
                             gpointer   user_data)
{
  GtkContainer *container = user_data;
  GList *list;
  GList *tmp;

  list = gtk_container_get_children (container);

  tmp = list;
  while (tmp != NULL)
    {
      /* don't disable our toggle */
      if (GTK_WIDGET (tmp->data) != togglebutton)
        gtk_widget_set_sensitive (GTK_WIDGET (tmp->data),
                                  !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (togglebutton)));

      tmp = tmp->next;
    }

  g_list_free (list);
}


GtkWidget *
do_images (GtkWidget *do_widget)
{
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *button;
  GIcon     *gicon;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Images");

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);
      g_signal_connect (window, "destroy",
                        G_CALLBACK (cleanup_callback), NULL);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
      g_object_set (vbox, "margin", 16, NULL);
      gtk_container_add (GTK_CONTAINER (window), vbox);

      label = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (label),
                            "<u>Image loaded from a file</u>");
      gtk_box_pack_start (GTK_BOX (vbox), label);

      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
      gtk_widget_set_halign (frame, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (frame, GTK_ALIGN_CENTER);
      gtk_box_pack_start (GTK_BOX (vbox), frame);

      image = gtk_image_new_from_icon_name ("gtk3-demo");
      gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);

      gtk_container_add (GTK_CONTAINER (frame), image);


      /* Animation */

      label = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (label),
                            "<u>Animation loaded from a file</u>");
      gtk_box_pack_start (GTK_BOX (vbox), label);

      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
      gtk_widget_set_halign (frame, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (frame, GTK_ALIGN_CENTER);
      gtk_box_pack_start (GTK_BOX (vbox), frame);

      image = gtk_image_new_from_resource ("/images/floppybuddy.gif");

      gtk_container_add (GTK_CONTAINER (frame), image);

      /* Symbolic icon */

      label = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (label),
                            "<u>Symbolic themed icon</u>");
      gtk_box_pack_start (GTK_BOX (vbox), label);

      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
      gtk_widget_set_halign (frame, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (frame, GTK_ALIGN_CENTER);
      gtk_box_pack_start (GTK_BOX (vbox), frame);

      gicon = g_themed_icon_new_with_default_fallbacks ("battery-caution-charging-symbolic");
      image = gtk_image_new_from_gicon (gicon);
      gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);

      gtk_container_add (GTK_CONTAINER (frame), image);


      /* Progressive */

      label = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (label),
                            "<u>Progressive image loading</u>");
      gtk_box_pack_start (GTK_BOX (vbox), label);

      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
      gtk_widget_set_halign (frame, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (frame, GTK_ALIGN_CENTER);
      gtk_box_pack_start (GTK_BOX (vbox), frame);

      /* Create an empty image for now; the progressive loader
       * will create the pixbuf and fill it in.
       */
      image = gtk_image_new_from_pixbuf (NULL);
      gtk_container_add (GTK_CONTAINER (frame), image);

      start_progressive_loading (image);

      /* Sensitivity control */
      button = gtk_toggle_button_new_with_mnemonic ("_Insensitive");
      gtk_box_pack_start (GTK_BOX (vbox), button);

      g_signal_connect (button, "toggled",
                        G_CALLBACK (toggle_sensitivity_callback),
                        vbox);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
