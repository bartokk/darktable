/*
    This file is part of darktable,
    copyright (c) 2009--2010 henrik andersson.

    darktable is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    darktable is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with darktable.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "common/darktable.h"
#include "common/styles.h"
#include "common/history.h"
#include "develop/imageop.h"
#include "control/control.h"
#include "gui/styles.h"
#include "gui/gtk.h"
#include "gui/hist_dialog.h"

typedef enum _style_items_columns_t
{
  DT_HIST_ITEMS_COL_ENABLED=0,
  DT_HIST_ITEMS_COL_NAME,
  DT_HIST_ITEMS_COL_NUM,
  DT_HIST_ITEMS_NUM_COLS
}
_styles_columns_t;

static GList *
_gui_hist_get_active_items (dt_gui_hist_dialog_t *d)
{
  GList *result=NULL;

  /* run thru all items and add active ones to result */
  GtkTreeIter iter;
  GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (d->items));
  if (gtk_tree_model_get_iter_first(model,&iter))
  {
    do
    {
      gboolean active;
      guint num=0;
      gtk_tree_model_get (model, &iter, DT_HIST_ITEMS_COL_ENABLED, &active, DT_HIST_ITEMS_COL_NUM, &num, -1);
      if (active)
        result = g_list_append (result, (gpointer)(long unsigned int) num);

    }
    while (gtk_tree_model_iter_next (model,&iter));
  }
  return result;
}

static void
_gui_hist_set_items (dt_gui_hist_dialog_t *d, gboolean active)
{
  /* run thru all items and set active status */
  GtkTreeIter iter;
  GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (d->items));
  if (gtk_tree_model_get_iter_first(model,&iter))
  {
    do
    {
      gtk_list_store_set (GTK_LIST_STORE (model), &iter, DT_HIST_ITEMS_COL_ENABLED, active, -1);
    }
    while (gtk_tree_model_iter_next (model,&iter));
  }
}

static void
_gui_hist_copy_response(GtkDialog *dialog, gint response_id, dt_gui_hist_dialog_t *g)
{
  switch(response_id)
  {
  case GTK_RESPONSE_CANCEL:
    break;

  case GTK_RESPONSE_YES:
    _gui_hist_set_items (g, TRUE);
    break;

  case GTK_RESPONSE_NONE:
    _gui_hist_set_items (g, FALSE);
    break;

  case GTK_RESPONSE_OK:
    g->selops = _gui_hist_get_active_items(g);
    break;
  }
}

static void
_gui_hist_item_toggled (GtkCellRendererToggle *cell,
                        gchar                 *path_str,
                        gpointer               data)
{
  dt_gui_hist_dialog_t *d = (dt_gui_hist_dialog_t *)data;

  GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (d->items));
  GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
  GtkTreeIter iter;
  gboolean toggle_item;

  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, DT_HIST_ITEMS_COL_ENABLED, &toggle_item, -1);

  toggle_item = (toggle_item==TRUE)?FALSE:TRUE;

  gtk_list_store_set (GTK_LIST_STORE (model), &iter, DT_HIST_ITEMS_COL_ENABLED, toggle_item, -1);
  gtk_tree_path_free (path);
}

void dt_gui_hist_dialog_new (dt_gui_hist_dialog_t *d, int imgid, gboolean iscopy)
{
  GtkWidget *window = dt_ui_main_window(darktable.gui->ui);
  char label[10];

  if (iscopy)
  {
    strcpy(label, _("copy all"));
    d->copied_imageid = imgid;
  }
  else
    strcpy(label, _("paste all"));

  GtkDialog *dialog = GTK_DIALOG
    (gtk_dialog_new_with_buttons
     ("select parts",
      GTK_WINDOW(window),
      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
      GTK_STOCK_CANCEL,
      GTK_RESPONSE_CANCEL,
      GTK_STOCK_SELECT_ALL,
      GTK_RESPONSE_YES,
      GTK_STOCK_CLEAR,
      GTK_RESPONSE_NONE,
      GTK_STOCK_OK,
      GTK_RESPONSE_OK,
      NULL));

  GtkContainer *content_area = GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog)));
  GtkWidget *alignment = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
  gtk_alignment_set_padding (GTK_ALIGNMENT(alignment), 5, 5, 5, 5);
  gtk_container_add (content_area, alignment);
  GtkBox *box = GTK_BOX (gtk_vbox_new(FALSE, 3));
  gtk_container_add (GTK_CONTAINER (alignment), GTK_WIDGET (box));

  /* create the list of items */
  d->items = GTK_TREE_VIEW (gtk_tree_view_new ());
  GtkListStore *liststore = gtk_list_store_new (DT_HIST_ITEMS_NUM_COLS, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_UINT);

  /* enabled */
  GtkCellRenderer *renderer = gtk_cell_renderer_toggle_new ();
  gtk_cell_renderer_toggle_set_activatable (GTK_CELL_RENDERER_TOGGLE (renderer), TRUE);
  g_object_set_data (G_OBJECT (renderer), "column", (gint *)DT_HIST_ITEMS_COL_ENABLED);
  g_signal_connect (renderer, "toggled", G_CALLBACK (_gui_hist_item_toggled), d);

  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (d->items),
      -1, _("include"),
      renderer,
      "active",
      DT_HIST_ITEMS_COL_ENABLED,
      NULL);

  /* name */
  renderer = gtk_cell_renderer_text_new ();
  g_object_set_data (G_OBJECT (renderer), "column", (gint *)DT_HIST_ITEMS_COL_NAME);
  g_object_set (renderer, "xalign", 0.0, NULL);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (d->items),
      -1, _("item"),
      renderer,
      "text",
      DT_HIST_ITEMS_COL_NAME,
      NULL);


  gtk_tree_selection_set_mode (gtk_tree_view_get_selection(GTK_TREE_VIEW(d->items)), GTK_SELECTION_SINGLE);
  gtk_tree_view_set_model (GTK_TREE_VIEW(d->items), GTK_TREE_MODEL(liststore));

  gtk_box_pack_start (box,GTK_WIDGET (d->items),TRUE,TRUE,0);

  /* fill list with history items */
  GtkTreeIter iter;
  GList *items = dt_history_get_items (imgid, TRUE);
  if (items)
  {
    do
    {
      dt_history_item_t *item = (dt_history_item_t *)items->data;

      gtk_list_store_append (GTK_LIST_STORE(liststore), &iter);
      gtk_list_store_set (GTK_LIST_STORE(liststore), &iter,
                          DT_HIST_ITEMS_COL_ENABLED, TRUE,
                          DT_HIST_ITEMS_COL_NAME, item->name,
                          DT_HIST_ITEMS_COL_NUM, (guint)item->num,
                          -1);

      g_free(item->op);
      g_free(item->name);
      g_free(item);
    }
    while ((items=g_list_next(items)));
  }
  else
  {
    dt_control_log(_("can't copy history out of unaltered image"));
    return;
  }

  g_object_unref (liststore);

  g_signal_connect (dialog, "response", G_CALLBACK (_gui_hist_copy_response), d);

  gtk_widget_show_all (GTK_WIDGET (dialog));

  while(1)
  {
    int res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_CANCEL || res == GTK_RESPONSE_OK)
      break;
  }

  gtk_widget_destroy(GTK_WIDGET(dialog));
}

// modelines: These editor modelines have been set for all relevant files by tools/update_modelines.sh
// vim: shiftwidth=2 expandtab tabstop=2 cindent
// kate: tab-indents: off; indent-width 2; replace-tabs on; indent-mode cstyle; remove-trailing-space on;