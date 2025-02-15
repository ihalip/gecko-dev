/*
 * Copyright © 2007  Chris Wilson
 * Copyright © 2009,2010  Red Hat, Inc.
 * Copyright © 2011,2012  Google, Inc.
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
 * Contributor(s):
 *	Chris Wilson <chris@chris-wilson.co.uk>
 * Red Hat Author(s): Behdad Esfahbod
 * Google Author(s): Behdad Esfahbod
 */

#ifndef HB_OBJECT_PRIVATE_HH
#define HB_OBJECT_PRIVATE_HH

#include "hb-private.hh"
#include "hb-atomic-private.hh"
#include "hb-mutex-private.hh"
#include "hb-vector-private.hh"


/*
 * Lockable set
 */

template <typename item_t, typename lock_t>
struct hb_lockable_set_t
{
  hb_vector_t <item_t, 1> items;

  inline void init (void) { items.init (); }

  template <typename T>
  inline item_t *replace_or_insert (T v, lock_t &l, bool replace)
  {
    l.lock ();
    item_t *item = items.find (v);
    if (item) {
      if (replace) {
	item_t old = *item;
	*item = v;
	l.unlock ();
	old.fini ();
      }
      else {
        item = nullptr;
	l.unlock ();
      }
    } else {
      item = items.push (v);
      l.unlock ();
    }
    return item;
  }

  template <typename T>
  inline void remove (T v, lock_t &l)
  {
    l.lock ();
    item_t *item = items.find (v);
    if (item) {
      item_t old = *item;
      *item = items[items.len - 1];
      items.pop ();
      l.unlock ();
      old.fini ();
    } else {
      l.unlock ();
    }
  }

  template <typename T>
  inline bool find (T v, item_t *i, lock_t &l)
  {
    l.lock ();
    item_t *item = items.find (v);
    if (item)
      *i = *item;
    l.unlock ();
    return !!item;
  }

  template <typename T>
  inline item_t *find_or_insert (T v, lock_t &l)
  {
    l.lock ();
    item_t *item = items.find (v);
    if (!item) {
      item = items.push (v);
    }
    l.unlock ();
    return item;
  }

  inline void fini (lock_t &l)
  {
    if (!items.len) {
      /* No need for locking. */
      items.fini ();
      return;
    }
    l.lock ();
    while (items.len) {
      item_t old = items[items.len - 1];
	items.pop ();
	l.unlock ();
	old.fini ();
	l.lock ();
    }
    items.fini ();
    l.unlock ();
  }

};


/*
 * Reference-count.
 */

#define HB_REFERENCE_COUNT_INERT_VALUE 0
#define HB_REFERENCE_COUNT_POISON_VALUE -0x0000DEAD
#define HB_REFERENCE_COUNT_INIT {HB_ATOMIC_INT_INIT (HB_REFERENCE_COUNT_INERT_VALUE)}

struct hb_reference_count_t
{
  hb_atomic_int_t ref_count;

  inline void init (int v) { ref_count.set_relaxed (v); }
  inline int get_relaxed (void) const { return ref_count.get_relaxed (); }
  inline int inc (void) { return ref_count.inc (); }
  inline int dec (void) { return ref_count.dec (); }
  inline void fini (void) { ref_count.set_relaxed (HB_REFERENCE_COUNT_POISON_VALUE); }

  inline bool is_inert (void) const { return ref_count.get_relaxed () == HB_REFERENCE_COUNT_INERT_VALUE; }
  inline bool is_valid (void) const { return ref_count.get_relaxed () > 0; }
};


/* user_data */

struct hb_user_data_array_t
{
  struct hb_user_data_item_t {
    hb_user_data_key_t *key;
    void *data;
    hb_destroy_func_t destroy;

    inline bool operator == (hb_user_data_key_t *other_key) const { return key == other_key; }
    inline bool operator == (hb_user_data_item_t &other) const { return key == other.key; }

    void fini (void) { if (destroy) destroy (data); }
  };

  hb_mutex_t lock;
  hb_lockable_set_t<hb_user_data_item_t, hb_mutex_t> items;

  inline void init (void) { lock.init (); items.init (); }

  HB_INTERNAL bool set (hb_user_data_key_t *key,
			void *              data,
			hb_destroy_func_t   destroy,
			hb_bool_t           replace);

  HB_INTERNAL void *get (hb_user_data_key_t *key);

  inline void fini (void) { items.fini (lock); lock.fini (); }
};


/*
 * Object header
 */

struct hb_object_header_t
{
  hb_reference_count_t ref_count;
  mutable hb_user_data_array_t *user_data;

#define HB_OBJECT_HEADER_STATIC {HB_REFERENCE_COUNT_INIT, nullptr}

  private:
  ASSERT_POD ();
};


/*
 * Object
 */

template <typename Type>
static inline void hb_object_trace (const Type *obj, const char *function)
{
  DEBUG_MSG (OBJECT, (void *) obj,
	     "%s refcount=%d",
	     function,
	     obj ? obj->header.ref_count.get_relaxed () : 0);
}

template <typename Type>
static inline Type *hb_object_create (void)
{
  Type *obj = (Type *) calloc (1, sizeof (Type));

  if (unlikely (!obj))
    return obj;

  hb_object_init (obj);
  hb_object_trace (obj, HB_FUNC);
  return obj;
}
template <typename Type>
static inline void hb_object_init (Type *obj)
{
  obj->header.ref_count.init (1);
  obj->header.user_data = nullptr;
}
template <typename Type>
static inline bool hb_object_is_inert (const Type *obj)
{
  return unlikely (obj->header.ref_count.is_inert ());
}
template <typename Type>
static inline bool hb_object_is_valid (const Type *obj)
{
  return likely (obj->header.ref_count.is_valid ());
}
template <typename Type>
static inline Type *hb_object_reference (Type *obj)
{
  hb_object_trace (obj, HB_FUNC);
  if (unlikely (!obj || hb_object_is_inert (obj)))
    return obj;
  assert (hb_object_is_valid (obj));
  obj->header.ref_count.inc ();
  return obj;
}
template <typename Type>
static inline bool hb_object_destroy (Type *obj)
{
  hb_object_trace (obj, HB_FUNC);
  if (unlikely (!obj || hb_object_is_inert (obj)))
    return false;
  assert (hb_object_is_valid (obj));
  if (obj->header.ref_count.dec () != 1)
    return false;

  hb_object_fini (obj);
  return true;
}
template <typename Type>
static inline void hb_object_fini (Type *obj)
{
  obj->header.ref_count.fini (); /* Do this before user_data */
  if (obj->header.user_data)
  {
    obj->header.user_data->fini ();
    free (obj->header.user_data);
  }
}
template <typename Type>
static inline bool hb_object_set_user_data (Type               *obj,
					    hb_user_data_key_t *key,
					    void *              data,
					    hb_destroy_func_t   destroy,
					    hb_bool_t           replace)
{
  if (unlikely (!obj || hb_object_is_inert (obj)))
    return false;
  assert (hb_object_is_valid (obj));

retry:
  hb_user_data_array_t *user_data = (hb_user_data_array_t *) hb_atomic_ptr_get (&obj->header.user_data);
  if (unlikely (!user_data))
  {
    user_data = (hb_user_data_array_t *) calloc (sizeof (hb_user_data_array_t), 1);
    if (unlikely (!user_data))
      return false;
    user_data->init ();
    if (unlikely (!hb_atomic_ptr_cmpexch (&obj->header.user_data, nullptr, user_data)))
    {
      user_data->fini ();
      free (user_data);
      goto retry;
    }
  }

  return user_data->set (key, data, destroy, replace);
}

template <typename Type>
static inline void *hb_object_get_user_data (Type               *obj,
					     hb_user_data_key_t *key)
{
  if (unlikely (!obj || hb_object_is_inert (obj) || !obj->header.user_data))
    return nullptr;
  assert (hb_object_is_valid (obj));
  return obj->header.user_data->get (key);
}


#endif /* HB_OBJECT_PRIVATE_HH */
