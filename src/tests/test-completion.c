/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 * test-completion.c
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2007 - Jesús Barbero Rodríguez <chuchiperriman@gmail.com>
 * Copyright (C) 2013 - Sébastien Wilmet <swilmet@gnome.org>
 *
 * GtkSourceView is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * GtkSourceView is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>
#include <gtksourceview/completion-providers/words/gtksourcecompletionwords.h>

static GtkWidget *view;
static GtkSourceCompletion *comp;

static const gboolean change_keys = FALSE;

typedef struct _TestProvider TestProvider;
typedef struct _TestProviderClass TestProviderClass;

struct _TestProvider
{
	GObject parent;

	GList *proposals;
	gint priority;
	gchar *name;

	GdkPixbuf *icon;

	/* If it's a random provider, a subset of 'proposals' are choosen on
	 * each populate. Otherwise, all the proposals are shown. */
	guint is_random : 1;
};

struct _TestProviderClass
{
	GObjectClass parent_class;
};

static void test_provider_iface_init (GtkSourceCompletionProviderIface *iface);
GType test_provider_get_type (void);

G_DEFINE_TYPE_WITH_CODE (TestProvider,
			 test_provider,
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER,
				 		test_provider_iface_init))

static gchar *
test_provider_get_name (GtkSourceCompletionProvider *provider)
{
	return g_strdup (((TestProvider *)provider)->name);
}

static gint
test_provider_get_priority (GtkSourceCompletionProvider *provider)
{
	return ((TestProvider *)provider)->priority;
}

static gboolean
test_provider_match (GtkSourceCompletionProvider *provider,
                     GtkSourceCompletionContext  *context)
{
	return TRUE;
}

static GList *
select_random_proposals (GList *all_proposals)
{
	GList *selection = NULL;
	GList *prop;

	for (prop = all_proposals; prop != NULL; prop = g_list_next (prop))
	{
		if (g_random_boolean ())
		{
			selection = g_list_prepend (selection, prop->data);
		}
	}

	return selection;
}

static void
test_provider_populate (GtkSourceCompletionProvider *completion_provider,
                        GtkSourceCompletionContext  *context)
{
	TestProvider *provider = (TestProvider *)completion_provider;
	GList *proposals;

	if (provider->is_random)
	{
		proposals = select_random_proposals (provider->proposals);
	}
	else
	{
		proposals = provider->proposals;
	}

	gtk_source_completion_context_add_proposals (context,
						     completion_provider,
						     proposals,
						     TRUE);
}

static GdkPixbuf *
test_provider_get_icon (GtkSourceCompletionProvider *provider)
{
	TestProvider *tp = (TestProvider *)provider;

	if (tp->icon == NULL)
	{
		GtkIconTheme *theme = gtk_icon_theme_get_default ();
		tp->icon = gtk_icon_theme_load_icon (theme, GTK_STOCK_DIALOG_INFO, 16, 0, NULL);
	}

	return tp->icon;
}

static void
test_provider_iface_init (GtkSourceCompletionProviderIface *iface)
{
	iface->get_name = test_provider_get_name;

	iface->populate = test_provider_populate;
	iface->match = test_provider_match;
	iface->get_priority = test_provider_get_priority;

	//iface->get_icon = test_provider_get_icon;
}

static void
test_provider_dispose (GObject *gobject)
{
	TestProvider *self = (TestProvider *)gobject;

	g_list_free_full (self->proposals, g_object_unref);
	self->proposals = NULL;

	g_clear_object (&self->icon);

	G_OBJECT_CLASS (test_provider_parent_class)->dispose (gobject);
}

static void
test_provider_finalize (GObject *gobject)
{
	TestProvider *self = (TestProvider *)gobject;

	g_free (self->name);
	self->name = NULL;

	G_OBJECT_CLASS (test_provider_parent_class)->finalize (gobject);
}

static void
test_provider_class_init (TestProviderClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->dispose = test_provider_dispose;
	gobject_class->finalize = test_provider_finalize;
}

static void
test_provider_init (TestProvider *self)
{
}

static void
test_provider_set_fixed (TestProvider *provider)
{
	GList *proposals = NULL;

	GdkPixbuf *icon = test_provider_get_icon (GTK_SOURCE_COMPLETION_PROVIDER (provider));

	proposals = g_list_prepend (proposals,
	                            gtk_source_completion_item_new ("Proposal 3",
								    "Proposal 3",
								    icon,
								    "This is the third proposal... \n"
								    "it is so cool it takes two lines to describe"));

	proposals = g_list_prepend (proposals,
	                            gtk_source_completion_item_new ("Proposal 2",
								    "Proposal 2",
								    icon,
								    "This is the second proposal..."));

	proposals = g_list_prepend (proposals,
	                            gtk_source_completion_item_new ("Proposal 1",
								    "Proposal 1",
								    icon,
								    NULL));

	provider->proposals = proposals;
	provider->is_random = 0;
}

static void
test_provider_set_random (TestProvider *provider)
{
	GdkPixbuf *icon = test_provider_get_icon (GTK_SOURCE_COMPLETION_PROVIDER (provider));

	GList *proposals = NULL;
	gint i;

	for (i = 0; i < 10; i++)
	{
		gchar *name = g_strdup_printf ("Proposal %d", i);

		proposals = g_list_prepend (proposals,
					    gtk_source_completion_item_new (name,
									    name,
									    icon,
									    NULL));

		g_free (name);
	}

	provider->proposals = proposals;
	provider->is_random = 1;
}

static void
remember_toggled_cb (GtkToggleButton *button,
		     gpointer user_data)
{
	g_object_set (comp, "remember-info-visibility",
		      gtk_toggle_button_get_active (button),
		      NULL);
}

static void
select_on_show_toggled_cb (GtkToggleButton *button,
			   gpointer user_data)
{
	g_object_set (comp, "select-on-show",
		      gtk_toggle_button_get_active (button),
		      NULL);
}

static void
show_headers_toggled_cb (GtkToggleButton *button,
			 gpointer user_data)
{
	g_object_set (comp, "show-headers",
		      gtk_toggle_button_get_active (button),
		      NULL);
}

static void
toggle_active_property (gpointer     source,
                        gpointer     dest,
                        const gchar *name)
{
	gboolean value;

	g_object_get (source, name, &value, NULL);
	g_object_set (dest, "active", value, NULL);
}

static void
show_icons_toggled_cb (GtkToggleButton *button,
		       gpointer user_data)
{
	g_object_set (comp, "show-icons",
		      gtk_toggle_button_get_active (button),
		      NULL);
}

static GtkWidget *
create_window (void)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *remember;
	GtkWidget *select_on_show;
	GtkWidget *show_headers;
	GtkWidget *show_icons;
	GtkSourceCompletion *completion;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_resize (GTK_WINDOW (window), 600, 400);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 1);
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 1);

	view = gtk_source_view_new ();
	GtkWidget *scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (scroll), view);

	remember = gtk_check_button_new_with_label ("Remember info visibility");
	select_on_show = gtk_check_button_new_with_label ("Select first on show");
	show_headers = gtk_check_button_new_with_label ("Show headers");
	show_icons = gtk_check_button_new_with_label ("Show icons");

	completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (view));

	toggle_active_property (completion, remember, "remember-info-visibility");
	toggle_active_property (completion, select_on_show, "select-on-show");
	toggle_active_property (completion, show_headers, "show-headers");
	toggle_active_property (completion, show_icons, "show-icons");

	gtk_box_pack_start (GTK_BOX (hbox), remember, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), select_on_show, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), show_headers, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), show_icons, FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (vbox), scroll, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	gtk_container_add (GTK_CONTAINER (window), vbox);

	g_signal_connect (window, "destroy",
			  G_CALLBACK (gtk_main_quit),
			   NULL);
	g_signal_connect (remember, "toggled",
			  G_CALLBACK (remember_toggled_cb),
			  NULL);
	g_signal_connect (select_on_show, "toggled",
			  G_CALLBACK (select_on_show_toggled_cb),
			  NULL);
	g_signal_connect (show_headers, "toggled",
			  G_CALLBACK (show_headers_toggled_cb),
			  NULL);
	g_signal_connect (show_icons, "toggled",
			  G_CALLBACK (show_icons_toggled_cb),
			  NULL);

	return window;
}

static void
create_completion (void)
{
	GtkSourceCompletionWords *prov_words;

	comp = gtk_source_view_get_completion (GTK_SOURCE_VIEW (view));

	/* Words completion provider */
	prov_words = gtk_source_completion_words_new (NULL, NULL);

	gtk_source_completion_words_register (prov_words,
	                                      gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));

	gtk_source_completion_add_provider (comp,
	                                    GTK_SOURCE_COMPLETION_PROVIDER (prov_words),
	                                    NULL);

	g_object_set (prov_words, "priority", 10, NULL);
	g_object_unref (prov_words);

	/* Fixed provider: the proposals don't change */
	TestProvider *tp = g_object_new (test_provider_get_type (), NULL);
	test_provider_set_fixed (tp);
	tp->priority = 5;
	tp->name = g_strdup ("Fixed Provider");

	gtk_source_completion_add_provider (comp,
	                                    GTK_SOURCE_COMPLETION_PROVIDER (tp),
	                                    NULL);
	g_object_unref (tp);

	/* Random provider: the proposals vary on each populate */
	tp = g_object_new (test_provider_get_type (), NULL);
	test_provider_set_random (tp);
	tp->priority = 1;
	tp->name = g_strdup ("Random Provider");

	gtk_source_completion_add_provider (comp,
	                                    GTK_SOURCE_COMPLETION_PROVIDER (tp),
	                                    NULL);
	g_object_unref (tp);
}

int
main (int argc, char *argv[])
{
	GtkWidget *window;

	gtk_init (&argc, &argv);

	window = create_window ();
	create_completion ();

	gtk_widget_show_all (window);

	gtk_main ();
	return 0;
}
