/* gdktexture.c
 *
 * Copyright 2016  Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:textures
 * @Title: GdkTexture
 * @Short_description: Pixel data
 *
 * #GdkTexture is the basic element used to refer to pixel data.
 * It is primarily mean for pixel data that will not change over
 * multiple frames, and will be used for a long time.
 *
 * You cannot get your pixel data back once you've uploaded it.
 *
 * #GdkTexture is an immutable object: That means you cannot change
 * anything about it other than increasing the reference count via
 * g_object_ref().
 */

#include "config.h"

#include "gdktextureprivate.h"

#include "gdkinternals.h"
#include "gdkcairo.h"

#include <epoxy/gl.h>

/**
 * SECTION:gdktexture
 * @Short_description: Image data for display
 * @Title: GdkTexture
 *
 * A GdkTexture represents image data that can be displayed on screen.
 *
 * There are various ways to create GdkTexture objects from a #GdkPixbuf
 * or a cairo surface, or other pixel data.
 *
 * An important aspect of GdkTextures is that they are immutable - once
 * the image data has been wrapped in a GdkTexture, it may be uploaded
 * to the GPU or used in other ways that make it impractical to allow
 * modification.
 */

/**
 * GdkTexture:
 *
 * The `GdkTexture` structure contains only private data.
 */

enum {
  PROP_0,
  PROP_WIDTH,
  PROP_HEIGHT,

  N_PROPS
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_ABSTRACT_TYPE (GdkTexture, gdk_texture, G_TYPE_OBJECT)

#define GDK_TEXTURE_WARN_NOT_IMPLEMENTED_METHOD(obj,method) \
  g_critical ("Texture of type '%s' does not implement GdkTexture::" # method, G_OBJECT_TYPE_NAME (obj))

static void
gdk_texture_real_download (GdkTexture *self,
                           guchar     *data,
                           gsize       stride)
{
  GDK_TEXTURE_WARN_NOT_IMPLEMENTED_METHOD (self, download);
}

static cairo_surface_t *
gdk_texture_real_download_surface (GdkTexture *texture)
{
  cairo_surface_t *surface;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        texture->width, texture->height);
  gdk_texture_download (texture,
                        cairo_image_surface_get_data (surface),
                        cairo_image_surface_get_stride (surface));
  cairo_surface_mark_dirty (surface);

  return surface;
}

static void
gdk_texture_set_property (GObject      *gobject,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GdkTexture *self = GDK_TEXTURE (gobject);

  switch (prop_id)
    {
    case PROP_WIDTH:
      self->width = g_value_get_int (value);
      break;

    case PROP_HEIGHT:
      self->height = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gdk_texture_get_property (GObject    *gobject,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GdkTexture *self = GDK_TEXTURE (gobject);

  switch (prop_id)
    {
    case PROP_WIDTH:
      g_value_set_int (value, self->width);
      break;

    case PROP_HEIGHT:
      g_value_set_int (value, self->height);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gdk_texture_dispose (GObject *object)
{
  GdkTexture *self = GDK_TEXTURE (object);

  gdk_texture_clear_render_data (self);

  G_OBJECT_CLASS (gdk_texture_parent_class)->dispose (object);
}

static void
gdk_texture_class_init (GdkTextureClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  klass->download = gdk_texture_real_download;
  klass->download_surface = gdk_texture_real_download_surface;

  gobject_class->set_property = gdk_texture_set_property;
  gobject_class->get_property = gdk_texture_get_property;
  gobject_class->dispose = gdk_texture_dispose;

  /**
   * GdkTexture:width:
   *
   * The width of the texture.
   */
  properties[PROP_WIDTH] =
    g_param_spec_int ("width",
                      "Width",
                      "The width of the texture",
                      1,
                      G_MAXINT,
                      1,
                      G_PARAM_READWRITE |
                      G_PARAM_CONSTRUCT_ONLY |
                      G_PARAM_STATIC_STRINGS |
                      G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GdkTexture:height:
   *
   * The height of the texture.
   */
  properties[PROP_HEIGHT] =
    g_param_spec_int ("height",
                      "Height",
                      "The height of the texture",
                      1,
                      G_MAXINT,
                      1,
                      G_PARAM_READWRITE |
                      G_PARAM_CONSTRUCT_ONLY |
                      G_PARAM_STATIC_STRINGS |
                      G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gdk_texture_init (GdkTexture *self)
{
}

/* GdkCairoTexture */

#define GDK_TYPE_CAIRO_TEXTURE (gdk_cairo_texture_get_type ())

G_DECLARE_FINAL_TYPE (GdkCairoTexture, gdk_cairo_texture, GDK, CAIRO_TEXTURE, GdkTexture)

struct _GdkCairoTexture {
  GdkTexture parent_instance;
  cairo_surface_t *surface;
};

struct _GdkCairoTextureClass {
  GdkTextureClass parent_class;
};

G_DEFINE_TYPE (GdkCairoTexture, gdk_cairo_texture, GDK_TYPE_TEXTURE)

static void
gdk_cairo_texture_finalize (GObject *object)
{
  GdkCairoTexture *self = GDK_CAIRO_TEXTURE (object);

  cairo_surface_destroy (self->surface);

  G_OBJECT_CLASS (gdk_cairo_texture_parent_class)->finalize (object);
}

static cairo_surface_t *
gdk_cairo_texture_download_surface (GdkTexture *texture)
{
  GdkCairoTexture *self = GDK_CAIRO_TEXTURE (texture);

  return cairo_surface_reference (self->surface);
}

static void
gdk_cairo_texture_download (GdkTexture *texture,
                            guchar     *data,
                            gsize       stride)
{
  GdkCairoTexture *self = GDK_CAIRO_TEXTURE (texture);
  cairo_surface_t *surface;
  cairo_t *cr;

  surface = cairo_image_surface_create_for_data (data,
                                                 CAIRO_FORMAT_ARGB32,
                                                 texture->width, texture->height,
                                                 stride);
  cr = cairo_create (surface);

  cairo_set_source_surface (cr, self->surface, 0, 0);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint (cr);

  cairo_destroy (cr);
  cairo_surface_finish (surface);
  cairo_surface_destroy (surface);
}

static void
gdk_cairo_texture_class_init (GdkCairoTextureClass *klass)
{
  GdkTextureClass *texture_class = GDK_TEXTURE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  texture_class->download = gdk_cairo_texture_download;
  texture_class->download_surface = gdk_cairo_texture_download_surface;

  gobject_class->finalize = gdk_cairo_texture_finalize;
}

static void
gdk_cairo_texture_init (GdkCairoTexture *self)
{
}

/**
 * gdk_texture_new_for_data:
 * @data: (array): the pixel data
 * @width: the number of pixels in each row
 * @height: the number of rows
 * @stride: the distance from the beginning of one row to the next, in bytes
 *
 * Creates a new texture object holding the given data.
 * The data is assumed to be in CAIRO_FORMAT_ARGB32 format.
 *
 * Returns: a new #GdkTexture
 */
GdkTexture *
gdk_texture_new_for_data (const guchar *data,
                          int           width,
                          int           height,
                          int           stride)
{
  GdkTexture *texture;
  cairo_surface_t *original, *copy;
  cairo_t *cr;

  g_return_val_if_fail (width > 0, NULL);
  g_return_val_if_fail (height > 0, NULL);

  original = cairo_image_surface_create_for_data ((guchar *) data, CAIRO_FORMAT_ARGB32, width, height, stride);
  copy = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);

  cr = cairo_create (copy);
  cairo_set_source_surface (cr, original, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);

  texture = gdk_texture_new_for_surface (copy);

  cairo_surface_destroy (copy);
  cairo_surface_finish (original);
  cairo_surface_destroy (original);

  return texture;
}

/**
 * gdk_texture_new_for_surface:
 * @surface: a cairo image surface
 *
 * Creates a new texture object representing the surface.
 * @surface must be an image surface with format CAIRO_FORMAT_ARGB32.
 *
 * Returns: a new #GdkTexture
 */
GdkTexture *
gdk_texture_new_for_surface (cairo_surface_t *surface)
{
  GdkCairoTexture *texture;

  g_return_val_if_fail (cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_IMAGE, NULL);
  g_return_val_if_fail (cairo_image_surface_get_width (surface) > 0, NULL);
  g_return_val_if_fail (cairo_image_surface_get_height (surface) > 0, NULL);

  texture = g_object_new (GDK_TYPE_CAIRO_TEXTURE,
                          "width", cairo_image_surface_get_width (surface),
                          "height", cairo_image_surface_get_height (surface),
                          NULL);

  texture->surface = cairo_surface_reference (surface);

  return (GdkTexture *) texture;
}

/* GdkPixbufTexture */

#define GDK_TYPE_PIXBUF_TEXTURE (gdk_pixbuf_texture_get_type ())

G_DECLARE_FINAL_TYPE (GdkPixbufTexture, gdk_pixbuf_texture, GDK, PIXBUF_TEXTURE, GdkTexture)

struct _GdkPixbufTexture {
  GdkTexture parent_instance;

  GdkPixbuf *pixbuf;
};

struct _GdkPixbufTextureClass {
  GdkTextureClass parent_class;
};

G_DEFINE_TYPE (GdkPixbufTexture, gdk_pixbuf_texture, GDK_TYPE_TEXTURE)

static void
gdk_pixbuf_texture_finalize (GObject *object)
{
  GdkPixbufTexture *self = GDK_PIXBUF_TEXTURE (object);

  g_object_unref (self->pixbuf);

  G_OBJECT_CLASS (gdk_pixbuf_texture_parent_class)->finalize (object);
}

static void
gdk_pixbuf_texture_download (GdkTexture *texture,
                             guchar     *data,
                             gsize       stride)
{
  GdkPixbufTexture *self = GDK_PIXBUF_TEXTURE (texture);
  cairo_surface_t *surface;

  surface = cairo_image_surface_create_for_data (data,
                                                 CAIRO_FORMAT_ARGB32,
                                                 texture->width, texture->height,
                                                 stride);
  gdk_cairo_surface_paint_pixbuf (surface, self->pixbuf);
  cairo_surface_finish (surface);
  cairo_surface_destroy (surface);
}

static cairo_surface_t *
gdk_pixbuf_texture_download_surface (GdkTexture *texture)
{
  GdkPixbufTexture *self = GDK_PIXBUF_TEXTURE (texture);

  return gdk_cairo_surface_create_from_pixbuf (self->pixbuf, 1, NULL);
}

static void
gdk_pixbuf_texture_class_init (GdkPixbufTextureClass *klass)
{
  GdkTextureClass *texture_class = GDK_TEXTURE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  texture_class->download = gdk_pixbuf_texture_download;
  texture_class->download_surface = gdk_pixbuf_texture_download_surface;

  gobject_class->finalize = gdk_pixbuf_texture_finalize;
}

static void
gdk_pixbuf_texture_init (GdkPixbufTexture *self)
{
}

/* GdkGLTexture */


struct _GdkGLTexture {
  GdkTexture parent_instance;

  GdkGLContext *context;
  guint id;

  cairo_surface_t *saved;

  GDestroyNotify destroy;
  gpointer data;
};

struct _GdkGLTextureClass {
  GdkTextureClass parent_class;
};

G_DEFINE_TYPE (GdkGLTexture, gdk_gl_texture, GDK_TYPE_TEXTURE)

static void
gdk_gl_texture_dispose (GObject *object)
{
  GdkGLTexture *self = GDK_GL_TEXTURE (object);

  if (self->destroy)
    {
      self->destroy (self->data);
      self->destroy = NULL;
      self->data = NULL;
    }

  g_clear_object (&self->context);
  self->id = 0;

  if (self->saved)
    {
      cairo_surface_destroy (self->saved);
      self->saved = NULL;
    }

  G_OBJECT_CLASS (gdk_gl_texture_parent_class)->dispose (object);
}

static void
gdk_gl_texture_download (GdkTexture *texture,
                         guchar     *data,
                         gsize       stride)
{
  GdkGLTexture *self = GDK_GL_TEXTURE (texture);
  cairo_surface_t *surface;
  cairo_t *cr;

  surface = cairo_image_surface_create_for_data (data,
                                                 CAIRO_FORMAT_ARGB32,
                                                 texture->width, texture->height,
                                                 stride);

  cr = cairo_create (surface);

  if (self->saved)
    {
      cairo_set_source_surface (cr, self->saved, 0, 0);
      cairo_paint (cr);
    }
  else
    {
      GdkWindow *window;

      window = gdk_gl_context_get_window (self->context);
      gdk_cairo_draw_from_gl (cr, window, self->id, GL_TEXTURE, 1, 0, 0,
                              texture->width, texture->height);
    }

  cairo_destroy (cr);
  cairo_surface_finish (surface);
  cairo_surface_destroy (surface);
}

static void
gdk_gl_texture_class_init (GdkGLTextureClass *klass)
{
  GdkTextureClass *texture_class = GDK_TEXTURE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  texture_class->download = gdk_gl_texture_download;
  gobject_class->dispose = gdk_gl_texture_dispose;
}

static void
gdk_gl_texture_init (GdkGLTexture *self)
{
}

GdkGLContext *
gdk_gl_texture_get_context (GdkGLTexture *self)
{
  return self->context;
}

guint
gdk_gl_texture_get_id (GdkGLTexture *self)
{
  return self->id;
}

/**
 * gdk_texture_release_gl:
 * @texture: a #GdkTexture wrapping a GL texture
 *
 * Releases the GL resources held by a #GdkTexture that
 * was created with gdk_texture_new_for_gl().
 *
 * The texture contents are still available via the
 * gdk_texture_download() function, after this function
 * has been called.
 */
void
gdk_texture_release_gl (GdkTexture *texture)
{
  GdkGLTexture *self;
  GdkWindow *window;
  cairo_t *cr;

  g_return_if_fail (GDK_IS_GL_TEXTURE (texture));

  self = GDK_GL_TEXTURE (texture);

  g_return_if_fail (self->saved == NULL);

  self->saved = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                            texture->width, texture->height);

  cr = cairo_create (self->saved);

  window = gdk_gl_context_get_window (self->context);
  gdk_cairo_draw_from_gl (cr, window, self->id, GL_TEXTURE, 1, 0, 0,
                          texture->width, texture->height);

  cairo_destroy (cr);

  if (self->destroy)
    {
      self->destroy (self->data);
      self->destroy = NULL;
      self->data = NULL;
    }

  g_clear_object (&self->context);
  self->id = 0;
}

/**
 * gdk_texture_new_for_pixbuf:
 * @pixbuf: a #GdkPixbuf
 *
 * Creates a new texture object representing the GdkPixbuf.
 *
 * Returns: a new #GdkTexture
 */
GdkTexture *
gdk_texture_new_for_pixbuf (GdkPixbuf *pixbuf)
{
  GdkPixbufTexture *self;

  g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

  self = g_object_new (GDK_TYPE_PIXBUF_TEXTURE,
                       "width", gdk_pixbuf_get_width (pixbuf),
                       "height", gdk_pixbuf_get_height (pixbuf),
                       NULL);

  self->pixbuf = g_object_ref (pixbuf);

  return GDK_TEXTURE (self);
}

/**
 * gdk_texture_new_from_resource:
 * @resource_path: the path of the resource file
 *
 * Creates a new texture by loading an image from a resource.
 * The file format is detected automatically.
 *
 * It is a fatal error if @resource_path does not specify a valid
 * image resource and the program will abort if that happens.
 * If you are unsure about the validity of a resource, use
 * gdk_texture_new_from_file() to load it.
 *
 * Return value: A newly-created texture
 */
GdkTexture *
gdk_texture_new_from_resource (const char *resource_path)
{
  GError *error = NULL;
  GdkTexture *texture;
  GdkPixbuf *pixbuf;

  g_return_val_if_fail (resource_path != NULL, NULL);

  pixbuf = gdk_pixbuf_new_from_resource (resource_path, &error);
  if (pixbuf == NULL)
    g_error ("Resource path %s is not a valid image: %s", resource_path, error->message);

  texture = gdk_texture_new_for_pixbuf (pixbuf);
  g_object_unref (pixbuf);

  return texture;
}

/**
 * gdk_texture_new_from_file:
 * @file: #GFile to load
 * @error: Return location for an error
 *
 * Creates a new texture by loading an image from a file.  The file format is
 * detected automatically. If %NULL is returned, then @error will be set.
 *
 * Return value: A newly-created #GdkTexture or %NULL if an error occured.
 **/
GdkTexture *
gdk_texture_new_from_file (GFile   *file,
                           GError **error)
{
  GdkTexture *texture;
  GdkPixbuf *pixbuf;
  GInputStream *stream;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  stream = G_INPUT_STREAM (g_file_read (file, NULL, error));
  if (stream == NULL)
    return NULL;

  pixbuf = gdk_pixbuf_new_from_stream (stream, NULL, error);
  g_object_unref (stream);
  if (pixbuf == NULL)
    return NULL;

  texture = gdk_texture_new_for_pixbuf (pixbuf);
  g_object_unref (pixbuf);

  return texture;
}

/**
 * gdk_texture_new_for_gl:
 * @context: a #GdkGLContext
 * @id: the ID of a texture that was created with @context
 * @width: the nominal width of the texture
 * @height: the nominal height of the texture
 * @destroy: a destroy notify that will be called when the GL resources
 *           are released
 * @data: data that gets passed to @destroy
 *
 * Creates a new texture for an existing GL texture.
 *
 * Note that the GL texture must not be modified until @destroy is called,
 * which will happen when the GdkTexture object is finalized, or due to
 * an explicit call of gdk_texture_release_gl().
 *
 * Return value: A newly-created #GdkTexture
 **/
GdkTexture *
gdk_texture_new_for_gl (GdkGLContext   *context,
                        guint           id,
                        int             width,
                        int             height,
                        GDestroyNotify  destroy,
                        gpointer        data)
{
  GdkGLTexture *self;

  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), NULL);
  g_return_val_if_fail (id != 0, NULL);
  g_return_val_if_fail (width > 0, NULL);
  g_return_val_if_fail (height > 0, NULL);

  self = g_object_new (GDK_TYPE_GL_TEXTURE,
                       "width", width,
                       "height", height,
                       NULL);

  self->context = g_object_ref (context);
  self->id = id;
  self->destroy = destroy;
  self->data = data;

  return GDK_TEXTURE (self);
}

/**
 * gdk_texture_get_width:
 * @texture: a #GdkTexture
 *
 * Returns the width of @texture.
 *
 * Returns: the width of the #GdkTexture
 */
int
gdk_texture_get_width (GdkTexture *texture)
{
  g_return_val_if_fail (GDK_IS_TEXTURE (texture), 0);

  return texture->width;
}

/**
 * gdk_texture_get_height:
 * @texture: a #GdkTexture
 *
 * Returns the height of the @texture.
 *
 * Returns: the height of the #GdkTexture
 */
int
gdk_texture_get_height (GdkTexture *texture)
{
  g_return_val_if_fail (GDK_IS_TEXTURE (texture), 0);

  return texture->height;
}

cairo_surface_t *
gdk_texture_download_surface (GdkTexture *texture)
{
  return GDK_TEXTURE_GET_CLASS (texture)->download_surface (texture);
}

/**
 * gdk_texture_download:
 * @texture: a #GdkTexture
 * @data: (array): pointer to enough memory to be filled with the
 *     downloaded data of @texture
 * @stride: rowstride in bytes
 *
 * Downloads the @texture into local memory. This may be
 * an expensive operation, as the actual texture data may
 * reside on a GPU or on a remote display server.
 *
 * The data format of the downloaded data is equivalent to
 * %CAIRO_FORMAT_ARGB32, so every downloaded pixel requires
 * 4 bytes of memory.
 *
 * Downloading a texture into a Cairo image surface:
 * |[<!-- language="C" -->
 * surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
 *                                       gdk_texture_get_width (texture),
 *                                       gdk_texture_get_height (texture));
 * gdk_texture_download (texture,
 *                       cairo_image_surface_get_data (surface),
 *                       cairo_image_surface_get_stride (surface));
 * cairo_surface_mark_dirty (surface);
 * ]|
 **/
void
gdk_texture_download (GdkTexture *texture,
                      guchar     *data,
                      gsize       stride)
{
  g_return_if_fail (GDK_IS_TEXTURE (texture));
  g_return_if_fail (data != NULL);
  g_return_if_fail (stride >= gdk_texture_get_width (texture) * 4);

  return GDK_TEXTURE_GET_CLASS (texture)->download (texture, data, stride);
}

gboolean
gdk_texture_set_render_data (GdkTexture     *self,
                             gpointer        key,
                             gpointer        data,
                             GDestroyNotify  notify)
{
  g_return_val_if_fail (data != NULL, FALSE);
 
  if (self->render_key != NULL)
    return FALSE;

  self->render_key = key;
  self->render_data = data;
  self->render_notify = notify;

  return TRUE;
}

void
gdk_texture_clear_render_data (GdkTexture *self)
{
  if (self->render_notify)
    self->render_notify (self->render_data);

  self->render_key = NULL;
  self->render_data = NULL;
  self->render_notify = NULL;
}

gpointer
gdk_texture_get_render_data (GdkTexture  *self,
                             gpointer     key)
{
  if (self->render_key != key)
    return NULL;

  return self->render_data;
}
