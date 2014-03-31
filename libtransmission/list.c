/*
 * This file Copyright (C) Mnemosyne LLC
 *
 * This file is licensed by the GPL version 2. Works owned by the
 * Transmission project are granted a special exemption to clause 2 (b)
 * so that the bulk of its code can remain under the MIT license.
 * This exemption does not extend to derived works not owned by
 * the Transmission project.
 *
 * $Id: list.c 13625 2012-12-05 17:29:46Z jordan $
 */

#include "transmission.h"
#include "list.h"
#include "utils.h"

static const tr_list TR_LIST_CLEAR = { NULL, NULL, NULL };

static tr_list * recycled_nodes = NULL;

static tr_list*
node_alloc (void)
{
  tr_list * ret;

  if (recycled_nodes == NULL)
    {
      ret = tr_new (tr_list, 1);
    }
  else
    {
      ret = recycled_nodes;
      recycled_nodes = recycled_nodes->next;
    }

  *ret = TR_LIST_CLEAR;
  return ret;
}

static void
node_free (tr_list* node)
{
  if (node != NULL)
    {
      *node = TR_LIST_CLEAR;
      node->next = recycled_nodes;
      recycled_nodes = node;
    }
}

/***
****
***/

void
tr_list_free (tr_list**         list,
              TrListForeachFunc data_free_func)
{
  while (*list)
    {
      tr_list *node = *list;
      *list = (*list)->next;
      if (data_free_func)
        data_free_func (node->data);
      node_free (node);
    }
}

void
tr_list_prepend (tr_list ** list,
                 void *     data)
{
  tr_list * node = node_alloc ();

  node->data = data;
  node->next = *list;
  if (*list)
    (*list)->prev = node;
  *list = node;
}

void
tr_list_append (tr_list ** list,
                void *     data)
{
  tr_list * node = node_alloc ();

  node->data = data;

  if (!*list)
    {
      *list = node;
    }
  else
    {
      tr_list * l = *list;

      while (l->next)
        l = l->next;

      l->next = node;
      node->prev = l;
    }
}

static tr_list*
tr_list_find_data (tr_list *    list,
                   const void * data)
{
  for (; list; list = list->next)
    if (list->data == data)
      return list;

  return NULL;
}

static void*
tr_list_remove_node (tr_list ** list,
                     tr_list *  node)
{
  void *    data;
  tr_list * prev = node ? node->prev : NULL;
  tr_list * next = node ? node->next : NULL;

  if (prev) prev->next = next;
  if (next) next->prev = prev;
  if (*list == node) *list = next;
  data = node ? node->data : NULL;
  node_free (node);
  return data;
}

void*
tr_list_pop_front (tr_list ** list)
{
  void * ret = NULL;

  if (*list)
    {
      ret = (*list)->data;
      tr_list_remove_node (list, *list);
    }

  return ret;
}

void*
tr_list_remove_data (tr_list **   list,
                     const void * data)
{
  return tr_list_remove_node (list, tr_list_find_data (*list, data));
}

void*
tr_list_remove (tr_list **        list,
                const void *      b,
                TrListCompareFunc compare_func)
{
  return tr_list_remove_node (list, tr_list_find (*list, b, compare_func));
}

tr_list*
tr_list_find (tr_list *         list,
              const void *      b,
              TrListCompareFunc func)
{
  for (; list; list = list->next)
    if (!func (list->data, b))
      return list;

  return NULL;
}

void
tr_list_insert_sorted (tr_list            ** list,
                       void                * data,
                       TrListCompareFunc     compare)
{
  /* find l, the node that we'll insert this data before */
  tr_list * l;

  for (l = *list; l != NULL; l = l->next)
    {
      const int c = (compare)(data, l->data);
      if (c <= 0)
        break;
    }

  if (l == NULL)
    {
      tr_list_append (list, data);
    }
  else if (l == *list)
    {
      tr_list_prepend (list, data);
    }
  else
    {
      tr_list * node = node_alloc ();
      node->data = data;
      node->prev = l->prev;
      node->next = l;
      node->prev->next = node;
      node->next->prev = node;
    }
}

int
tr_list_size (const tr_list * list)
{
  int size = 0;

  while (list)
    {
      ++size;
      list = list->next;
    }

  return size;
}
