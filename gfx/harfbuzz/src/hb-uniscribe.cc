/*
 * Copyright © 2011,2012,2013  Google, Inc.
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
 * Google Author(s): Behdad Esfahbod
 */

#include "hb-private.hh"
#define HB_SHAPER uniscribe
#include "hb-shaper-impl-private.hh"

#include <windows.h>
#include <usp10.h>
#include <rpc.h>

#include "hb-uniscribe.h"

#include "hb-open-file-private.hh"
#include "hb-ot-name-table.hh"
#include "hb-ot-tag.h"


static inline uint16_t hb_uint16_swap (const uint16_t v)
{ return (v >> 8) | (v << 8); }
static inline uint32_t hb_uint32_swap (const uint32_t v)
{ return (hb_uint16_swap (v) << 16) | hb_uint16_swap (v >> 16); }


typedef HRESULT (WINAPI *SIOT) /*ScriptItemizeOpenType*/(
  const WCHAR *pwcInChars,
  int cInChars,
  int cMaxItems,
  const SCRIPT_CONTROL *psControl,
  const SCRIPT_STATE *psState,
  SCRIPT_ITEM *pItems,
  OPENTYPE_TAG *pScriptTags,
  int *pcItems
);

typedef HRESULT (WINAPI *SSOT) /*ScriptShapeOpenType*/(
  HDC hdc,
  SCRIPT_CACHE *psc,
  SCRIPT_ANALYSIS *psa,
  OPENTYPE_TAG tagScript,
  OPENTYPE_TAG tagLangSys,
  int *rcRangeChars,
  TEXTRANGE_PROPERTIES **rpRangeProperties,
  int cRanges,
  const WCHAR *pwcChars,
  int cChars,
  int cMaxGlyphs,
  WORD *pwLogClust,
  SCRIPT_CHARPROP *pCharProps,
  WORD *pwOutGlyphs,
  SCRIPT_GLYPHPROP *pOutGlyphProps,
  int *pcGlyphs
);

typedef HRESULT (WINAPI *SPOT) /*ScriptPlaceOpenType*/(
  HDC hdc,
  SCRIPT_CACHE *psc,
  SCRIPT_ANALYSIS *psa,
  OPENTYPE_TAG tagScript,
  OPENTYPE_TAG tagLangSys,
  int *rcRangeChars,
  TEXTRANGE_PROPERTIES **rpRangeProperties,
  int cRanges,
  const WCHAR *pwcChars,
  WORD *pwLogClust,
  SCRIPT_CHARPROP *pCharProps,
  int cChars,
  const WORD *pwGlyphs,
  const SCRIPT_GLYPHPROP *pGlyphProps,
  int cGlyphs,
  int *piAdvance,
  GOFFSET *pGoffset,
  ABC *pABC
);


/* Fallback implementations. */

static HRESULT WINAPI
hb_ScriptItemizeOpenType(
  const WCHAR *pwcInChars,
  int cInChars,
  int cMaxItems,
  const SCRIPT_CONTROL *psControl,
  const SCRIPT_STATE *psState,
  SCRIPT_ITEM *pItems,
  OPENTYPE_TAG *pScriptTags,
  int *pcItems
)
{
{
  return ScriptItemize (pwcInChars,
			cInChars,
			cMaxItems,
			psControl,
			psState,
			pItems,
			pcItems);
}
}

static HRESULT WINAPI
hb_ScriptShapeOpenType(
  HDC hdc,
  SCRIPT_CACHE *psc,
  SCRIPT_ANALYSIS *psa,
  OPENTYPE_TAG tagScript,
  OPENTYPE_TAG tagLangSys,
  int *rcRangeChars,
  TEXTRANGE_PROPERTIES **rpRangeProperties,
  int cRanges,
  const WCHAR *pwcChars,
  int cChars,
  int cMaxGlyphs,
  WORD *pwLogClust,
  SCRIPT_CHARPROP *pCharProps,
  WORD *pwOutGlyphs,
  SCRIPT_GLYPHPROP *pOutGlyphProps,
  int *pcGlyphs
)
{
  SCRIPT_VISATTR *psva = (SCRIPT_VISATTR *) pOutGlyphProps;
  return ScriptShape (hdc,
		      psc,
		      pwcChars,
		      cChars,
		      cMaxGlyphs,
		      psa,
		      pwOutGlyphs,
		      pwLogClust,
		      psva,
		      pcGlyphs);
}

static HRESULT WINAPI
hb_ScriptPlaceOpenType(
  HDC hdc,
  SCRIPT_CACHE *psc,
  SCRIPT_ANALYSIS *psa,
  OPENTYPE_TAG tagScript,
  OPENTYPE_TAG tagLangSys,
  int *rcRangeChars,
  TEXTRANGE_PROPERTIES **rpRangeProperties,
  int cRanges,
  const WCHAR *pwcChars,
  WORD *pwLogClust,
  SCRIPT_CHARPROP *pCharProps,
  int cChars,
  const WORD *pwGlyphs,
  const SCRIPT_GLYPHPROP *pGlyphProps,
  int cGlyphs,
  int *piAdvance,
  GOFFSET *pGoffset,
  ABC *pABC
)
{
  SCRIPT_VISATTR *psva = (SCRIPT_VISATTR *) pGlyphProps;
  return ScriptPlace (hdc,
		      psc,
		      pwGlyphs,
		      cGlyphs,
		      psva,
		      psa,
		      piAdvance,
		      pGoffset,
		      pABC);
}


struct hb_uniscribe_shaper_funcs_t {
  SIOT ScriptItemizeOpenType;
  SSOT ScriptShapeOpenType;
  SPOT ScriptPlaceOpenType;

  inline void init (void)
  {
    HMODULE hinstLib;
    this->ScriptItemizeOpenType = nullptr;
    this->ScriptShapeOpenType   = nullptr;
    this->ScriptPlaceOpenType   = nullptr;

    hinstLib = GetModuleHandle (TEXT ("usp10.dll"));
    if (hinstLib)
    {
      this->ScriptItemizeOpenType = (SIOT) GetProcAddress (hinstLib, "ScriptItemizeOpenType");
      this->ScriptShapeOpenType   = (SSOT) GetProcAddress (hinstLib, "ScriptShapeOpenType");
      this->ScriptPlaceOpenType   = (SPOT) GetProcAddress (hinstLib, "ScriptPlaceOpenType");
    }
    if (!this->ScriptItemizeOpenType ||
	!this->ScriptShapeOpenType   ||
	!this->ScriptPlaceOpenType)
    {
      DEBUG_MSG (UNISCRIBE, nullptr, "OpenType versions of functions not found; falling back.");
      this->ScriptItemizeOpenType = hb_ScriptItemizeOpenType;
      this->ScriptShapeOpenType   = hb_ScriptShapeOpenType;
      this->ScriptPlaceOpenType   = hb_ScriptPlaceOpenType;
    }
  }
};
static hb_uniscribe_shaper_funcs_t *uniscribe_funcs;

#ifdef HB_USE_ATEXIT
static inline void
free_uniscribe_funcs (void)
{
retry:
  hb_uniscribe_shaper_funcs_t *local_uniscribe_funcs =
    (hb_uniscribe_shaper_funcs_t *) hb_atomic_ptr_get (&uniscribe_funcs);
  if (!hb_atomic_ptr_cmpexch (&uniscribe_funcs, local_uniscribe_funcs, nullptr))
    goto retry;

  free (uniscribe_funcs);
}
#endif

static hb_uniscribe_shaper_funcs_t *
hb_uniscribe_shaper_get_funcs (void)
{
retry:
  hb_uniscribe_shaper_funcs_t *funcs = (hb_uniscribe_shaper_funcs_t *) hb_atomic_ptr_get (&uniscribe_funcs);

  if (unlikely (!funcs))
  {
    funcs = (hb_uniscribe_shaper_funcs_t *) calloc (1, sizeof (hb_uniscribe_shaper_funcs_t));
    if (unlikely (!funcs))
      return nullptr;

    funcs->init ();

    if (!hb_atomic_ptr_cmpexch (&uniscribe_funcs, nullptr, funcs)) {
      free (funcs);
      goto retry;
    }

#ifdef HB_USE_ATEXIT
    atexit (free_uniscribe_funcs); /* First person registers atexit() callback. */
#endif
  }

  return funcs;
}


struct active_feature_t {
  OPENTYPE_FEATURE_RECORD rec;
  unsigned int order;

  static int cmp (const void *pa, const void *pb) {
    const active_feature_t *a = (const active_feature_t *) pa;
    const active_feature_t *b = (const active_feature_t *) pb;
    return a->rec.tagFeature < b->rec.tagFeature ? -1 : a->rec.tagFeature > b->rec.tagFeature ? 1 :
	   a->order < b->order ? -1 : a->order > b->order ? 1 :
	   a->rec.lParameter < b->rec.lParameter ? -1 : a->rec.lParameter > b->rec.lParameter ? 1 :
	   0;
  }
  bool operator== (const active_feature_t *f) {
    return cmp (this, f) == 0;
  }
};

struct feature_event_t {
  unsigned int index;
  bool start;
  active_feature_t feature;

  static int cmp (const void *pa, const void *pb) {
    const feature_event_t *a = (const feature_event_t *) pa;
    const feature_event_t *b = (const feature_event_t *) pb;
    return a->index < b->index ? -1 : a->index > b->index ? 1 :
	   a->start < b->start ? -1 : a->start > b->start ? 1 :
	   active_feature_t::cmp (&a->feature, &b->feature);
  }
};

struct range_record_t {
  TEXTRANGE_PROPERTIES props;
  unsigned int index_first; /* == start */
  unsigned int index_last;  /* == end - 1 */
};

HB_SHAPER_DATA_ENSURE_DEFINE(uniscribe, face)
HB_SHAPER_DATA_ENSURE_DEFINE(uniscribe, font)


/*
 * shaper face data
 */

struct hb_uniscribe_face_data_t {
  HANDLE fh;
  hb_uniscribe_shaper_funcs_t *funcs;
  wchar_t face_name[LF_FACESIZE];
};

/* face_name should point to a wchar_t[LF_FACESIZE] object. */
static void
_hb_generate_unique_face_name (wchar_t *face_name, unsigned int *plen)
{
  /* We'll create a private name for the font from a UUID using a simple,
   * somewhat base64-like encoding scheme */
  const char *enc = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-";
  UUID id;
  UuidCreate ((UUID*) &id);
  static_assert ((2 + 3 * (16/2) < LF_FACESIZE), "");
  unsigned int name_str_len = 0;
  face_name[name_str_len++] = 'F';
  face_name[name_str_len++] = '_';
  unsigned char *p = (unsigned char *) &id;
  for (unsigned int i = 0; i < 16; i += 2)
  {
    /* Spread the 16 bits from two bytes of the UUID across three chars of face_name,
     * using the bits in groups of 5,5,6 to select chars from enc.
     * This will generate 24 characters; with the 'F_' prefix we already provided,
     * the name will be 26 chars (plus the NUL terminator), so will always fit within
     * face_name (LF_FACESIZE = 32). */
    face_name[name_str_len++] = enc[p[i] >> 3];
    face_name[name_str_len++] = enc[((p[i] << 2) | (p[i + 1] >> 6)) & 0x1f];
    face_name[name_str_len++] = enc[p[i + 1] & 0x3f];
  }
  face_name[name_str_len] = 0;
  if (plen)
    *plen = name_str_len;
}

/* Destroys blob. */
static hb_blob_t *
_hb_rename_font (hb_blob_t *blob, wchar_t *new_name)
{
  /* Create a copy of the font data, with the 'name' table replaced by a
   * table that names the font with our private F_* name created above.
   * For simplicity, we just append a new 'name' table and update the
   * sfnt directory; the original table is left in place, but unused.
   *
   * The new table will contain just 5 name IDs: family, style, unique,
   * full, PS. All of them point to the same name data with our unique name.
   */

  blob = hb_sanitize_context_t ().sanitize_blob<OT::OpenTypeFontFile> (blob);

  unsigned int length, new_length, name_str_len;
  const char *orig_sfnt_data = hb_blob_get_data (blob, &length);

  _hb_generate_unique_face_name (new_name, &name_str_len);

  static const uint16_t name_IDs[] = { 1, 2, 3, 4, 6 };

  unsigned int name_table_length = OT::name::min_size +
                                   ARRAY_LENGTH (name_IDs) * OT::NameRecord::static_size +
                                   name_str_len * 2; /* for name data in UTF16BE form */
  unsigned int padded_name_table_length = ((name_table_length + 3) & ~3);
  unsigned int name_table_offset = (length + 3) & ~3;

  new_length = name_table_offset + padded_name_table_length;
  void *new_sfnt_data = calloc (1, new_length);
  if (!new_sfnt_data)
  {
    hb_blob_destroy (blob);
    return nullptr;
  }

  memcpy(new_sfnt_data, orig_sfnt_data, length);

  OT::name &name = StructAtOffset<OT::name> (new_sfnt_data, name_table_offset);
  name.format.set (0);
  name.count.set (ARRAY_LENGTH (name_IDs));
  name.stringOffset.set (name.get_size ());
  for (unsigned int i = 0; i < ARRAY_LENGTH (name_IDs); i++)
  {
    OT::NameRecord &record = name.nameRecord[i];
    record.platformID.set (3);
    record.encodingID.set (1);
    record.languageID.set (0x0409u); /* English */
    record.nameID.set (name_IDs[i]);
    record.length.set (name_str_len * 2);
    record.offset.set (0);
  }

  /* Copy string data from new_name, converting wchar_t to UTF16BE. */
  unsigned char *p = &StructAfter<unsigned char> (name);
  for (unsigned int i = 0; i < name_str_len; i++)
  {
    *p++ = new_name[i] >> 8;
    *p++ = new_name[i] & 0xff;
  }

  /* Adjust name table entry to point to new name table */
  const OT::OpenTypeFontFile &file = * (OT::OpenTypeFontFile *) (new_sfnt_data);
  unsigned int face_count = file.get_face_count ();
  for (unsigned int face_index = 0; face_index < face_count; face_index++)
  {
    /* Note: doing multiple edits (ie. TTC) can be unsafe.  There may be
     * toe-stepping.  But we don't really care. */
    const OT::OpenTypeFontFace &face = file.get_face (face_index);
    unsigned int index;
    if (face.find_table_index (HB_OT_TAG_name, &index))
    {
      OT::TableRecord &record = const_cast<OT::TableRecord &> (face.get_table (index));
      record.checkSum.set_for_data (&name, padded_name_table_length);
      record.offset.set (name_table_offset);
      record.length.set (name_table_length);
    }
    else if (face_index == 0) /* Fail if first face doesn't have 'name' table. */
    {
      free (new_sfnt_data);
      hb_blob_destroy (blob);
      return nullptr;
    }
  }

  /* The checkSumAdjustment field in the 'head' table is now wrong,
   * but that doesn't actually seem to cause any problems so we don't
   * bother. */

  hb_blob_destroy (blob);
  return hb_blob_create ((const char *) new_sfnt_data, new_length,
			 HB_MEMORY_MODE_WRITABLE, nullptr, free);
}

hb_uniscribe_face_data_t *
_hb_uniscribe_shaper_face_data_create (hb_face_t *face)
{
  hb_uniscribe_face_data_t *data = (hb_uniscribe_face_data_t *) calloc (1, sizeof (hb_uniscribe_face_data_t));
  if (unlikely (!data))
    return nullptr;

  data->funcs = hb_uniscribe_shaper_get_funcs ();
  if (unlikely (!data->funcs))
  {
    free (data);
    return nullptr;
  }

  hb_blob_t *blob = hb_face_reference_blob (face);
  if (unlikely (!hb_blob_get_length (blob)))
    DEBUG_MSG (UNISCRIBE, face, "Face has empty blob");

  blob = _hb_rename_font (blob, data->face_name);
  if (unlikely (!blob))
  {
    free (data);
    return nullptr;
  }

  DWORD num_fonts_installed;
  data->fh = AddFontMemResourceEx ((void *) hb_blob_get_data (blob, nullptr),
				   hb_blob_get_length (blob),
				   0, &num_fonts_installed);
  if (unlikely (!data->fh))
  {
    DEBUG_MSG (UNISCRIBE, face, "Face AddFontMemResourceEx() failed");
    free (data);
    return nullptr;
  }

  return data;
}

void
_hb_uniscribe_shaper_face_data_destroy (hb_uniscribe_face_data_t *data)
{
  RemoveFontMemResourceEx (data->fh);
  free (data);
}


/*
 * shaper font data
 */

struct hb_uniscribe_font_data_t {
  HDC hdc;
  LOGFONTW log_font;
  HFONT hfont;
  SCRIPT_CACHE script_cache;
  double x_mult, y_mult; /* From LOGFONT space to HB space. */
};

static bool
populate_log_font (LOGFONTW  *lf,
		   hb_font_t *font,
		   unsigned int font_size)
{
  memset (lf, 0, sizeof (*lf));
  lf->lfHeight = - (int) font_size;
  lf->lfCharSet = DEFAULT_CHARSET;

  hb_face_t *face = font->face;
  hb_uniscribe_face_data_t *face_data = HB_SHAPER_DATA_GET (face);

  memcpy (lf->lfFaceName, face_data->face_name, sizeof (lf->lfFaceName));

  return true;
}

hb_uniscribe_font_data_t *
_hb_uniscribe_shaper_font_data_create (hb_font_t *font)
{
  if (unlikely (!hb_uniscribe_shaper_face_data_ensure (font->face))) return nullptr;

  hb_uniscribe_font_data_t *data = (hb_uniscribe_font_data_t *) calloc (1, sizeof (hb_uniscribe_font_data_t));
  if (unlikely (!data))
    return nullptr;

  int font_size = font->face->get_upem (); /* Default... */
  /* No idea if the following is even a good idea. */
  if (font->y_ppem)
    font_size = font->y_ppem;

  if (font_size < 0)
    font_size = -font_size;
  data->x_mult = (double) font->x_scale / font_size;
  data->y_mult = (double) font->y_scale / font_size;

  data->hdc = GetDC (nullptr);

  if (unlikely (!populate_log_font (&data->log_font, font, font_size))) {
    DEBUG_MSG (UNISCRIBE, font, "Font populate_log_font() failed");
    _hb_uniscribe_shaper_font_data_destroy (data);
    return nullptr;
  }

  data->hfont = CreateFontIndirectW (&data->log_font);
  if (unlikely (!data->hfont)) {
    DEBUG_MSG (UNISCRIBE, font, "Font CreateFontIndirectW() failed");
    _hb_uniscribe_shaper_font_data_destroy (data);
     return nullptr;
  }

  if (!SelectObject (data->hdc, data->hfont)) {
    DEBUG_MSG (UNISCRIBE, font, "Font SelectObject() failed");
    _hb_uniscribe_shaper_font_data_destroy (data);
     return nullptr;
  }

  return data;
}

void
_hb_uniscribe_shaper_font_data_destroy (hb_uniscribe_font_data_t *data)
{
  if (data->hdc)
    ReleaseDC (nullptr, data->hdc);
  if (data->hfont)
    DeleteObject (data->hfont);
  if (data->script_cache)
    ScriptFreeCache (&data->script_cache);
  free (data);
}

LOGFONTW *
hb_uniscribe_font_get_logfontw (hb_font_t *font)
{
  if (unlikely (!hb_uniscribe_shaper_font_data_ensure (font))) return nullptr;
  hb_uniscribe_font_data_t *font_data =  HB_SHAPER_DATA_GET (font);
  return &font_data->log_font;
}

HFONT
hb_uniscribe_font_get_hfont (hb_font_t *font)
{
  if (unlikely (!hb_uniscribe_shaper_font_data_ensure (font))) return nullptr;
  hb_uniscribe_font_data_t *font_data =  HB_SHAPER_DATA_GET (font);
  return font_data->hfont;
}


/*
 * shaper shape_plan data
 */

struct hb_uniscribe_shape_plan_data_t {};

hb_uniscribe_shape_plan_data_t *
_hb_uniscribe_shaper_shape_plan_data_create (hb_shape_plan_t    *shape_plan HB_UNUSED,
					     const hb_feature_t *user_features HB_UNUSED,
					     unsigned int        num_user_features HB_UNUSED,
					     const int          *coords HB_UNUSED,
					     unsigned int        num_coords HB_UNUSED)
{
  return (hb_uniscribe_shape_plan_data_t *) HB_SHAPER_DATA_SUCCEEDED;
}

void
_hb_uniscribe_shaper_shape_plan_data_destroy (hb_uniscribe_shape_plan_data_t *data HB_UNUSED)
{
}


/*
 * shaper
 */


hb_bool_t
_hb_uniscribe_shape (hb_shape_plan_t    *shape_plan,
		     hb_font_t          *font,
		     hb_buffer_t        *buffer,
		     const hb_feature_t *features,
		     unsigned int        num_features)
{
  hb_face_t *face = font->face;
  hb_uniscribe_face_data_t *face_data = HB_SHAPER_DATA_GET (face);
  hb_uniscribe_font_data_t *font_data = HB_SHAPER_DATA_GET (font);
  hb_uniscribe_shaper_funcs_t *funcs = face_data->funcs;

  /*
   * Set up features.
   */
  hb_auto_t<hb_vector_t<OPENTYPE_FEATURE_RECORD> > feature_records;
  hb_auto_t<hb_vector_t<range_record_t> > range_records;
  if (num_features)
  {
    /* Sort features by start/end events. */
    hb_auto_t<hb_vector_t<feature_event_t> > feature_events;
    for (unsigned int i = 0; i < num_features; i++)
    {
      active_feature_t feature;
      feature.rec.tagFeature = hb_uint32_swap (features[i].tag);
      feature.rec.lParameter = features[i].value;
      feature.order = i;

      feature_event_t *event;

      event = feature_events.push ();
      event->index = features[i].start;
      event->start = true;
      event->feature = feature;

      event = feature_events.push ();
      event->index = features[i].end;
      event->start = false;
      event->feature = feature;
    }
    feature_events.qsort ();
    /* Add a strategic final event. */
    {
      active_feature_t feature;
      feature.rec.tagFeature = 0;
      feature.rec.lParameter = 0;
      feature.order = num_features + 1;

      feature_event_t *event = feature_events.push ();
      event->index = 0; /* This value does magic. */
      event->start = false;
      event->feature = feature;
    }

    /* Scan events and save features for each range. */
    hb_auto_t<hb_vector_t<active_feature_t> > active_features;
    unsigned int last_index = 0;
    for (unsigned int i = 0; i < feature_events.len; i++)
    {
      feature_event_t *event = &feature_events[i];

      if (event->index != last_index)
      {
        /* Save a snapshot of active features and the range. */
	range_record_t *range = range_records.push ();

	unsigned int offset = feature_records.len;

	active_features.qsort ();
	for (unsigned int j = 0; j < active_features.len; j++)
	{
	  if (!j || active_features[j].rec.tagFeature != feature_records[feature_records.len - 1].tagFeature)
	  {
	    feature_records.push (active_features[j].rec);
	  }
	  else
	  {
	    /* Overrides value for existing feature. */
	    feature_records[feature_records.len - 1].lParameter = active_features[j].rec.lParameter;
	  }
	}

	/* Will convert to pointer after all is ready, since feature_records.array
	 * may move as we grow it. */
	range->props.potfRecords = reinterpret_cast<OPENTYPE_FEATURE_RECORD *> (offset);
	range->props.cotfRecords = feature_records.len - offset;
	range->index_first = last_index;
	range->index_last  = event->index - 1;

	last_index = event->index;
      }

      if (event->start)
      {
	active_features.push (event->feature);
      }
      else
      {
        active_feature_t *feature = active_features.find (&event->feature);
	if (feature)
	  active_features.remove (feature - active_features.arrayZ);
      }
    }

    if (!range_records.len) /* No active feature found. */
      num_features = 0;

    /* Fixup the pointers. */
    for (unsigned int i = 0; i < range_records.len; i++)
    {
      range_record_t *range = &range_records[i];
      range->props.potfRecords = feature_records.arrayZ + reinterpret_cast<uintptr_t> (range->props.potfRecords);
    }
  }

#define FAIL(...) \
  HB_STMT_START { \
    DEBUG_MSG (UNISCRIBE, nullptr, __VA_ARGS__); \
    return false; \
  } HB_STMT_END;

  HRESULT hr;

retry:

  unsigned int scratch_size;
  hb_buffer_t::scratch_buffer_t *scratch = buffer->get_scratch_buffer (&scratch_size);

#define ALLOCATE_ARRAY(Type, name, len) \
  Type *name = (Type *) scratch; \
  { \
    unsigned int _consumed = DIV_CEIL ((len) * sizeof (Type), sizeof (*scratch)); \
    assert (_consumed <= scratch_size); \
    scratch += _consumed; \
    scratch_size -= _consumed; \
  }

#define utf16_index() var1.u32

  ALLOCATE_ARRAY (WCHAR, pchars, buffer->len * 2);

  unsigned int chars_len = 0;
  for (unsigned int i = 0; i < buffer->len; i++)
  {
    hb_codepoint_t c = buffer->info[i].codepoint;
    buffer->info[i].utf16_index() = chars_len;
    if (likely (c <= 0xFFFFu))
      pchars[chars_len++] = c;
    else if (unlikely (c > 0x10FFFFu))
      pchars[chars_len++] = 0xFFFDu;
    else {
      pchars[chars_len++] = 0xD800u + ((c - 0x10000u) >> 10);
      pchars[chars_len++] = 0xDC00u + ((c - 0x10000u) & ((1u << 10) - 1));
    }
  }

  ALLOCATE_ARRAY (WORD, log_clusters, chars_len);
  ALLOCATE_ARRAY (SCRIPT_CHARPROP, char_props, chars_len);

  if (num_features)
  {
    /* Need log_clusters to assign features. */
    chars_len = 0;
    for (unsigned int i = 0; i < buffer->len; i++)
    {
      hb_codepoint_t c = buffer->info[i].codepoint;
      unsigned int cluster = buffer->info[i].cluster;
      log_clusters[chars_len++] = cluster;
      if (hb_in_range (c, 0x10000u, 0x10FFFFu))
	log_clusters[chars_len++] = cluster; /* Surrogates. */
    }
  }

  /* The -2 in the following is to compensate for possible
   * alignment needed after the WORD array.  sizeof(WORD) == 2. */
  unsigned int glyphs_size = (scratch_size * sizeof (int) - 2)
			   / (sizeof (WORD) +
			      sizeof (SCRIPT_GLYPHPROP) +
			      sizeof (int) +
			      sizeof (GOFFSET) +
			      sizeof (uint32_t));

  ALLOCATE_ARRAY (WORD, glyphs, glyphs_size);
  ALLOCATE_ARRAY (SCRIPT_GLYPHPROP, glyph_props, glyphs_size);
  ALLOCATE_ARRAY (int, advances, glyphs_size);
  ALLOCATE_ARRAY (GOFFSET, offsets, glyphs_size);
  ALLOCATE_ARRAY (uint32_t, vis_clusters, glyphs_size);

  /* Note:
   * We can't touch the contents of glyph_props.  Our fallback
   * implementations of Shape and Place functions use that buffer
   * by casting it to a different type.  It works because they
   * both agree about it, but if we want to access it here we
   * need address that issue first.
   */

#undef ALLOCATE_ARRAY

#define MAX_ITEMS 256

  SCRIPT_ITEM items[MAX_ITEMS + 1];
  SCRIPT_CONTROL bidi_control = {0};
  SCRIPT_STATE bidi_state = {0};
  ULONG script_tags[MAX_ITEMS];
  int item_count;

  /* MinGW32 doesn't define fMergeNeutralItems, so we bruteforce */
  //bidi_control.fMergeNeutralItems = true;
  *(uint32_t*)&bidi_control |= 1u<<24;

  bidi_state.uBidiLevel = HB_DIRECTION_IS_FORWARD (buffer->props.direction) ? 0 : 1;
  bidi_state.fOverrideDirection = 1;

  hr = funcs->ScriptItemizeOpenType (pchars,
				     chars_len,
				     MAX_ITEMS,
				     &bidi_control,
				     &bidi_state,
				     items,
				     script_tags,
				     &item_count);
  if (unlikely (FAILED (hr)))
    FAIL ("ScriptItemizeOpenType() failed: 0x%08xL", hr);

#undef MAX_ITEMS

  OPENTYPE_TAG language_tag = hb_uint32_swap (hb_ot_tag_from_language (buffer->props.language));
  hb_auto_t<hb_vector_t<TEXTRANGE_PROPERTIES*> > range_properties;
  hb_auto_t<hb_vector_t<int> > range_char_counts;

  unsigned int glyphs_offset = 0;
  unsigned int glyphs_len;
  bool backward = HB_DIRECTION_IS_BACKWARD (buffer->props.direction);
  for (int i = 0; i < item_count; i++)
  {
    unsigned int chars_offset = items[i].iCharPos;
    unsigned int item_chars_len = items[i + 1].iCharPos - chars_offset;

    if (num_features)
    {
      range_properties.shrink (0);
      range_char_counts.shrink (0);

      range_record_t *last_range = &range_records[0];

      for (unsigned int k = chars_offset; k < chars_offset + item_chars_len; k++)
      {
	range_record_t *range = last_range;
	while (log_clusters[k] < range->index_first)
	  range--;
	while (log_clusters[k] > range->index_last)
	  range++;
	if (!range_properties.len ||
	    &range->props != range_properties[range_properties.len - 1])
	{
	  TEXTRANGE_PROPERTIES **props = range_properties.push ();
	  int *c = range_char_counts.push ();
	  if (unlikely (!props || !c))
	  {
	    range_properties.shrink (0);
	    range_char_counts.shrink (0);
	    break;
	  }
	  *props = &range->props;
	  *c = 1;
	}
	else
	{
	  range_char_counts[range_char_counts.len - 1]++;
	}

	last_range = range;
      }
    }

    /* Asking for glyphs in logical order circumvents at least
     * one bug in Uniscribe. */
    items[i].a.fLogicalOrder = true;

  retry_shape:
    hr = funcs->ScriptShapeOpenType (font_data->hdc,
				     &font_data->script_cache,
				     &items[i].a,
				     script_tags[i],
				     language_tag,
				     range_char_counts.arrayZ,
				     range_properties.arrayZ,
				     range_properties.len,
				     pchars + chars_offset,
				     item_chars_len,
				     glyphs_size - glyphs_offset,
				     /* out */
				     log_clusters + chars_offset,
				     char_props + chars_offset,
				     glyphs + glyphs_offset,
				     glyph_props + glyphs_offset,
				     (int *) &glyphs_len);

    if (unlikely (items[i].a.fNoGlyphIndex))
      FAIL ("ScriptShapeOpenType() set fNoGlyphIndex");
    if (unlikely (hr == E_OUTOFMEMORY || hr == E_NOT_SUFFICIENT_BUFFER))
    {
      if (unlikely (!buffer->ensure (buffer->allocated * 2)))
	FAIL ("Buffer resize failed");
      goto retry;
    }
    if (unlikely (hr == USP_E_SCRIPT_NOT_IN_FONT))
    {
      if (items[i].a.eScript == SCRIPT_UNDEFINED)
	FAIL ("ScriptShapeOpenType() failed: Font doesn't support script");
      items[i].a.eScript = SCRIPT_UNDEFINED;
      goto retry_shape;
    }
    if (unlikely (FAILED (hr)))
    {
      FAIL ("ScriptShapeOpenType() failed: 0x%08xL", hr);
    }

    for (unsigned int j = chars_offset; j < chars_offset + item_chars_len; j++)
      log_clusters[j] += glyphs_offset;

    hr = funcs->ScriptPlaceOpenType (font_data->hdc,
				     &font_data->script_cache,
				     &items[i].a,
				     script_tags[i],
				     language_tag,
				     range_char_counts.arrayZ,
				     range_properties.arrayZ,
				     range_properties.len,
				     pchars + chars_offset,
				     log_clusters + chars_offset,
				     char_props + chars_offset,
				     item_chars_len,
				     glyphs + glyphs_offset,
				     glyph_props + glyphs_offset,
				     glyphs_len,
				     /* out */
				     advances + glyphs_offset,
				     offsets + glyphs_offset,
				     nullptr);
    if (unlikely (FAILED (hr)))
      FAIL ("ScriptPlaceOpenType() failed: 0x%08xL", hr);

    if (DEBUG_ENABLED (UNISCRIBE))
      fprintf (stderr, "Item %d RTL %d LayoutRTL %d LogicalOrder %d ScriptTag %c%c%c%c\n",
	       i,
	       items[i].a.fRTL,
	       items[i].a.fLayoutRTL,
	       items[i].a.fLogicalOrder,
	       HB_UNTAG (hb_uint32_swap (script_tags[i])));

    glyphs_offset += glyphs_len;
  }
  glyphs_len = glyphs_offset;

  /* Ok, we've got everything we need, now compose output buffer,
   * very, *very*, carefully! */

  /* Calculate visual-clusters.  That's what we ship. */
  for (unsigned int i = 0; i < glyphs_len; i++)
    vis_clusters[i] = -1;
  for (unsigned int i = 0; i < buffer->len; i++) {
    uint32_t *p = &vis_clusters[log_clusters[buffer->info[i].utf16_index()]];
    *p = MIN (*p, buffer->info[i].cluster);
  }
  for (unsigned int i = 1; i < glyphs_len; i++)
    if (vis_clusters[i] == -1)
      vis_clusters[i] = vis_clusters[i - 1];

#undef utf16_index

  if (unlikely (!buffer->ensure (glyphs_len)))
    FAIL ("Buffer in error");

#undef FAIL

  /* Set glyph infos */
  buffer->len = 0;
  for (unsigned int i = 0; i < glyphs_len; i++)
  {
    hb_glyph_info_t *info = &buffer->info[buffer->len++];

    info->codepoint = glyphs[i];
    info->cluster = vis_clusters[i];

    /* The rest is crap.  Let's store position info there for now. */
    info->mask = advances[i];
    info->var1.i32 = offsets[i].du;
    info->var2.i32 = offsets[i].dv;
  }

  /* Set glyph positions */
  buffer->clear_positions ();
  double x_mult = font_data->x_mult, y_mult = font_data->y_mult;
  for (unsigned int i = 0; i < glyphs_len; i++)
  {
    hb_glyph_info_t *info = &buffer->info[i];
    hb_glyph_position_t *pos = &buffer->pos[i];

    /* TODO vertical */
    pos->x_advance = x_mult * (int32_t) info->mask;
    pos->x_offset = x_mult * (backward ? -info->var1.i32 : info->var1.i32);
    pos->y_offset = y_mult * info->var2.i32;
  }

  if (backward)
    hb_buffer_reverse (buffer);

  buffer->unsafe_to_break_all ();

  /* Wow, done! */
  return true;
}


