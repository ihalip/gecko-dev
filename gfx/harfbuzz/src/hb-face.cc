/*
 * Copyright © 2009  Red Hat, Inc.
 * Copyright © 2012  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Red Hat Author(s): Behdad Esfahbod
 * Google Author(s): Behdad Esfahbod
 */

#include "hb-private.hh"

#include "hb-face-private.hh"
#include "hb-blob-private.hh"
#include "hb-open-file-private.hh"


/**
 * hb_face_count: Get number of faces on the blob
 * @blob:
 *
 *
 *
 * Return value: Number of faces on the blob
 *
 * Since: 1.7.7
 **/
unsigned int
hb_face_count (hb_blob_t *blob)
{
  if (unlikely (!blob))
    return 0;

  /* TODO We shouldn't be sanitizing blob.  Port to run sanitizer and return if not sane. */
  /* Make API signature const after. */
  hb_blob_t *sanitized = hb_sanitize_context_t ().sanitize_blob<OT::OpenTypeFontFile> (hb_blob_reference (blob));
  const OT::OpenTypeFontFile& ot = *sanitized->as<OT::OpenTypeFontFile> ();
  unsigned int ret = ot.get_face_count ();
  hb_blob_destroy (sanitized);

  return ret;
}

/*
 * hb_face_t
 */

DEFINE_NULL_INSTANCE (hb_face_t) =
{
  HB_OBJECT_HEADER_STATIC,

  true, /* immutable */

  nullptr, /* reference_table_func */
  nullptr, /* user_data */
  nullptr, /* destroy */

  0,    /* index */
  1000, /* upem */
  0,    /* num_glyphs */

  {
#define HB_SHAPER_IMPLEMENT(shaper) HB_SHAPER_DATA_INVALID,
#include "hb-shaper-list.hh"
#undef HB_SHAPER_IMPLEMENT
  },

  nullptr, /* shape_plans */
};


/**
 * hb_face_create_for_tables:
 * @reference_table_func: (closure user_data) (destroy destroy) (scope notified):
 * @user_data:
 * @destroy:
 *
 *
 *
 * Return value: (transfer full)
 *
 * Since: 0.9.2
 **/
hb_face_t *
hb_face_create_for_tables (hb_reference_table_func_t  reference_table_func,
			   void                      *user_data,
			   hb_destroy_func_t          destroy)
{
  hb_face_t *face;

  if (!reference_table_func || !(face = hb_object_create<hb_face_t> ())) {
    if (destroy)
      destroy (user_data);
    return hb_face_get_empty ();
  }

  face->reference_table_func = reference_table_func;
  face->user_data = user_data;
  face->destroy = destroy;

  face->upem = 0;
  face->num_glyphs = (unsigned int) -1;

  return face;
}


typedef struct hb_face_for_data_closure_t {
  hb_blob_t *blob;
  unsigned int  index;
} hb_face_for_data_closure_t;

static hb_face_for_data_closure_t *
_hb_face_for_data_closure_create (hb_blob_t *blob, unsigned int index)
{
  hb_face_for_data_closure_t *closure;

  closure = (hb_face_for_data_closure_t *) calloc (1, sizeof (hb_face_for_data_closure_t));
  if (unlikely (!closure))
    return nullptr;

  closure->blob = blob;
  closure->index = index;

  return closure;
}

static void
_hb_face_for_data_closure_destroy (void *data)
{
  hb_face_for_data_closure_t *closure = (hb_face_for_data_closure_t *) data;

  hb_blob_destroy (closure->blob);
  free (closure);
}

static hb_blob_t *
_hb_face_for_data_reference_table (hb_face_t *face HB_UNUSED, hb_tag_t tag, void *user_data)
{
  hb_face_for_data_closure_t *data = (hb_face_for_data_closure_t *) user_data;

  if (tag == HB_TAG_NONE)
    return hb_blob_reference (data->blob);

  const OT::OpenTypeFontFile &ot_file = *data->blob->as<OT::OpenTypeFontFile> ();
  const OT::OpenTypeFontFace &ot_face = ot_file.get_face (data->index);

  const OT::OpenTypeTable &table = ot_face.get_table_by_tag (tag);

  hb_blob_t *blob = hb_blob_create_sub_blob (data->blob, table.offset, table.length);

  return blob;
}

/**
 * hb_face_create: (Xconstructor)
 * @blob:
 * @index:
 *
 *
 *
 * Return value: (transfer full):
 *
 * Since: 0.9.2
 **/
hb_face_t *
hb_face_create (hb_blob_t    *blob,
		unsigned int  index)
{
  hb_face_t *face;

  if (unlikely (!blob))
    blob = hb_blob_get_empty ();

  hb_face_for_data_closure_t *closure = _hb_face_for_data_closure_create (hb_sanitize_context_t ().sanitize_blob<OT::OpenTypeFontFile> (hb_blob_reference (blob)), index);

  if (unlikely (!closure))
    return hb_face_get_empty ();

  face = hb_face_create_for_tables (_hb_face_for_data_reference_table,
				    closure,
				    _hb_face_for_data_closure_destroy);

  face->index = index;

  return face;
}

/**
 * hb_face_get_empty:
 *
 *
 *
 * Return value: (transfer full)
 *
 * Since: 0.9.2
 **/
hb_face_t *
hb_face_get_empty (void)
{
  return const_cast<hb_face_t *> (&Null(hb_face_t));
}


/**
 * hb_face_reference: (skip)
 * @face: a face.
 *
 *
 *
 * Return value:
 *
 * Since: 0.9.2
 **/
hb_face_t *
hb_face_reference (hb_face_t *face)
{
  return hb_object_reference (face);
}

/**
 * hb_face_destroy: (skip)
 * @face: a face.
 *
 *
 *
 * Since: 0.9.2
 **/
void
hb_face_destroy (hb_face_t *face)
{
  if (!hb_object_destroy (face)) return;

  for (hb_face_t::plan_node_t *node = face->shape_plans; node; )
  {
    hb_face_t::plan_node_t *next = node->next;
    hb_shape_plan_destroy (node->shape_plan);
    free (node);
    node = next;
  }

#define HB_SHAPER_IMPLEMENT(shaper) HB_SHAPER_DATA_DESTROY(shaper, face);
#include "hb-shaper-list.hh"
#undef HB_SHAPER_IMPLEMENT

  if (face->destroy)
    face->destroy (face->user_data);

  free (face);
}

/**
 * hb_face_set_user_data: (skip)
 * @face: a face.
 * @key:
 * @data:
 * @destroy:
 * @replace:
 *
 *
 *
 * Return value:
 *
 * Since: 0.9.2
 **/
hb_bool_t
hb_face_set_user_data (hb_face_t          *face,
		       hb_user_data_key_t *key,
		       void *              data,
		       hb_destroy_func_t   destroy,
		       hb_bool_t           replace)
{
  return hb_object_set_user_data (face, key, data, destroy, replace);
}

/**
 * hb_face_get_user_data: (skip)
 * @face: a face.
 * @key:
 *
 *
 *
 * Return value: (transfer none):
 *
 * Since: 0.9.2
 **/
void *
hb_face_get_user_data (const hb_face_t    *face,
		       hb_user_data_key_t *key)
{
  return hb_object_get_user_data (face, key);
}

/**
 * hb_face_make_immutable:
 * @face: a face.
 *
 *
 *
 * Since: 0.9.2
 **/
void
hb_face_make_immutable (hb_face_t *face)
{
  if (unlikely (hb_object_is_inert (face)))
    return;

  face->immutable = true;
}

/**
 * hb_face_is_immutable:
 * @face: a face.
 *
 *
 *
 * Return value:
 *
 * Since: 0.9.2
 **/
hb_bool_t
hb_face_is_immutable (const hb_face_t *face)
{
  return face->immutable;
}


/**
 * hb_face_reference_table:
 * @face: a face.
 * @tag:
 *
 *
 *
 * Return value: (transfer full):
 *
 * Since: 0.9.2
 **/
hb_blob_t *
hb_face_reference_table (const hb_face_t *face,
			 hb_tag_t tag)
{
  return face->reference_table (tag);
}

/**
 * hb_face_reference_blob:
 * @face: a face.
 *
 *
 *
 * Return value: (transfer full):
 *
 * Since: 0.9.2
 **/
hb_blob_t *
hb_face_reference_blob (hb_face_t *face)
{
  return face->reference_table (HB_TAG_NONE);
}

/**
 * hb_face_set_index:
 * @face: a face.
 * @index:
 *
 *
 *
 * Since: 0.9.2
 **/
void
hb_face_set_index (hb_face_t    *face,
		   unsigned int  index)
{
  if (face->immutable)
    return;

  face->index = index;
}

/**
 * hb_face_get_index:
 * @face: a face.
 *
 *
 *
 * Return value:
 *
 * Since: 0.9.2
 **/
unsigned int
hb_face_get_index (const hb_face_t *face)
{
  return face->index;
}

/**
 * hb_face_set_upem:
 * @face: a face.
 * @upem:
 *
 *
 *
 * Since: 0.9.2
 **/
void
hb_face_set_upem (hb_face_t    *face,
		  unsigned int  upem)
{
  if (face->immutable)
    return;

  face->upem = upem;
}

/**
 * hb_face_get_upem:
 * @face: a face.
 *
 *
 *
 * Return value:
 *
 * Since: 0.9.2
 **/
unsigned int
hb_face_get_upem (const hb_face_t *face)
{
  return face->get_upem ();
}

/**
 * hb_face_set_glyph_count:
 * @face: a face.
 * @glyph_count:
 *
 *
 *
 * Since: 0.9.7
 **/
void
hb_face_set_glyph_count (hb_face_t    *face,
			 unsigned int  glyph_count)
{
  if (face->immutable)
    return;

  face->num_glyphs = glyph_count;
}

/**
 * hb_face_get_glyph_count:
 * @face: a face.
 *
 *
 *
 * Return value:
 *
 * Since: 0.9.7
 **/
unsigned int
hb_face_get_glyph_count (const hb_face_t *face)
{
  return face->get_num_glyphs ();
}

/**
 * hb_face_get_table_tags:
 * @face: a face.
 *
 * Retrieves table tags for a face, if possible.
 *
 * Return value: total number of tables, or 0 if not possible to list.
 *
 * Since: 1.6.0
 **/
unsigned int
hb_face_get_table_tags (const hb_face_t *face,
			unsigned int  start_offset,
			unsigned int *table_count, /* IN/OUT */
			hb_tag_t     *table_tags /* OUT */)
{
  if (face->destroy != (hb_destroy_func_t) _hb_face_for_data_closure_destroy)
  {
    if (table_count)
      *table_count = 0;
    return 0;
  }

  hb_face_for_data_closure_t *data = (hb_face_for_data_closure_t *) face->user_data;

  const OT::OpenTypeFontFile &ot_file = *data->blob->as<OT::OpenTypeFontFile> ();
  const OT::OpenTypeFontFace &ot_face = ot_file.get_face (data->index);

  return ot_face.get_table_tags (start_offset, table_count, table_tags);
}
