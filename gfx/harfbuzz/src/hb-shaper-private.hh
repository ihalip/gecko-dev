/*
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
 * Google Author(s): Behdad Esfahbod
 */

#ifndef HB_SHAPER_PRIVATE_HH
#define HB_SHAPER_PRIVATE_HH

#include "hb-private.hh"

typedef hb_bool_t hb_shape_func_t (hb_shape_plan_t    *shape_plan,
				   hb_font_t          *font,
				   hb_buffer_t        *buffer,
				   const hb_feature_t *features,
				   unsigned int        num_features);

#define HB_SHAPER_IMPLEMENT(name) \
	extern "C" HB_INTERNAL hb_shape_func_t _hb_##name##_shape;
#include "hb-shaper-list.hh"
#undef HB_SHAPER_IMPLEMENT

struct hb_shaper_pair_t {
  char name[16];
  hb_shape_func_t *func;
};

HB_INTERNAL const hb_shaper_pair_t *
_hb_shapers_get (void);


/* Means: succeeded, but don't need to keep any data. */
#define HB_SHAPER_DATA_SUCCEEDED ((void *) +1)
/* Means: tried but failed to create. */
#define HB_SHAPER_DATA_INVALID ((void *) -1)

#define HB_SHAPER_DATA_TYPE_NAME(shaper, object)	hb_##shaper##_##object##_data_t
#define HB_SHAPER_DATA_TYPE(shaper, object)		struct HB_SHAPER_DATA_TYPE_NAME(shaper, object)
#define HB_SHAPER_DATA_INSTANCE(shaper, object, instance)	(* (HB_SHAPER_DATA_TYPE(shaper, object) **) &(instance)->shaper_data.shaper)
#define HB_SHAPER_DATA(shaper, object)			HB_SHAPER_DATA_INSTANCE(shaper, object, object)
#define HB_SHAPER_DATA_CREATE_FUNC(shaper, object)	_hb_##shaper##_shaper_##object##_data_create
#define HB_SHAPER_DATA_DESTROY_FUNC(shaper, object)	_hb_##shaper##_shaper_##object##_data_destroy
#define HB_SHAPER_DATA_ENSURE_FUNC(shaper, object)	hb_##shaper##_shaper_##object##_data_ensure

#define HB_SHAPER_DATA_PROTOTYPE(shaper, object) \
	HB_SHAPER_DATA_TYPE (shaper, object); /* Type forward declaration. */ \
	extern "C" HB_INTERNAL HB_SHAPER_DATA_TYPE (shaper, object) * \
	HB_SHAPER_DATA_CREATE_FUNC (shaper, object) (hb_##object##_t *object HB_SHAPER_DATA_CREATE_FUNC_EXTRA_ARGS); \
	extern "C" HB_INTERNAL void \
	HB_SHAPER_DATA_DESTROY_FUNC (shaper, object) (HB_SHAPER_DATA_TYPE (shaper, object) *data); \
	extern "C" HB_INTERNAL bool \
	HB_SHAPER_DATA_ENSURE_FUNC (shaper, object) (hb_##object##_t *object)

#define HB_SHAPER_DATA_DESTROY(shaper, object) \
    if (HB_SHAPER_DATA_TYPE (shaper, object) *data = HB_SHAPER_DATA (shaper, object)) \
      if (data != HB_SHAPER_DATA_INVALID && data != HB_SHAPER_DATA_SUCCEEDED) \
        HB_SHAPER_DATA_DESTROY_FUNC (shaper, object) (data);

#define HB_SHAPER_DATA_ENSURE_DEFINE(shaper, object) \
	HB_SHAPER_DATA_ENSURE_DEFINE_WITH_CONDITION(shaper, object, true)

#define HB_SHAPER_DATA_ENSURE_DEFINE_WITH_CONDITION(shaper, object, condition) \
bool \
HB_SHAPER_DATA_ENSURE_FUNC(shaper, object) (hb_##object##_t *object) \
{\
  retry: \
  HB_SHAPER_DATA_TYPE (shaper, object) *data = (HB_SHAPER_DATA_TYPE (shaper, object) *) hb_atomic_ptr_get (&HB_SHAPER_DATA (shaper, object)); \
  if (likely (data) && !(condition)) { \
    /* Note that evaluating condition above can be dangerous if another thread \
     * got here first and destructed data.  That's, as always, bad use pattern. \
     * If you modify the font (change font size), other threads must not be \
     * using it at the same time.  However, since this check is delayed to \
     * when one actually tries to shape something, this is a XXX race condition \
     * (and the only know we have that I know of) right now.  Ie. you modify the \
     * font size in one thread, then (supposedly safely) try to use it from two \
     * or more threads and BOOM!  I'm not sure how to fix this.  We want RCU. \
     * Maybe when it doesn't matter when we finally implement AAT shaping, as
     * this (condition) is currently only used by hb-coretext. */ \
    /* Drop and recreate. */ \
    /* If someone dropped it in the mean time, throw it away and don't touch it. \
     * Otherwise, destruct it. */ \
    if (hb_atomic_ptr_cmpexch (&HB_SHAPER_DATA (shaper, object), data, nullptr)) { \
      HB_SHAPER_DATA_DESTROY_FUNC (shaper, object) (data); \
    } \
    goto retry; \
  } \
  if (unlikely (!data)) { \
    data = HB_SHAPER_DATA_CREATE_FUNC (shaper, object) (object); \
    if (unlikely (!data)) \
      data = (HB_SHAPER_DATA_TYPE (shaper, object) *) HB_SHAPER_DATA_INVALID; \
    if (!hb_atomic_ptr_cmpexch (&HB_SHAPER_DATA (shaper, object), nullptr, data)) { \
      if (data && \
	  data != HB_SHAPER_DATA_INVALID && \
	  data != HB_SHAPER_DATA_SUCCEEDED) \
	HB_SHAPER_DATA_DESTROY_FUNC (shaper, object) (data); \
      goto retry; \
    } \
  } \
  return data != nullptr && (void *) data != HB_SHAPER_DATA_INVALID; \
}


/* For embedding in face / font / ... */
struct hb_shaper_data_t {
#define HB_SHAPER_IMPLEMENT(shaper) void *shaper;
#include "hb-shaper-list.hh"
#undef HB_SHAPER_IMPLEMENT
};
#define HB_SHAPERS_COUNT (sizeof (hb_shaper_data_t) / sizeof (void *))


#endif /* HB_SHAPER_PRIVATE_HH */
