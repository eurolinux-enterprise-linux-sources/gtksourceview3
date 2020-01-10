/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- */
/* gtksourcebuffer.c
 * This file is part of GtkSourceView
 *
 * Copyright (C) 1999-2002 - Mikael Hermansson <tyan@linux.se>,
 *                           Chris Phelps <chicane@reninet.com> and
 *                           Jeroen Zwartepoorte <jeroen@xs4all.nl>
 * Copyright (C) 2003 - Paolo Maggi <paolo.maggi@polito.it> and
 *                      Gustavo Giráldez <gustavo.giraldez@gmx.net>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>

#include "gtksourcebuffer.h"
#include "gtksourcebuffer-private.h"
#include "gtksourcelanguage.h"
#include "gtksourcelanguage-private.h"
#include "gtksourceundomanager.h"
#include "gtksourceundomanagerdefault.h"
#include "gtksourcestylescheme.h"
#include "gtksourcestyleschememanager.h"
#include "gtksourcestyle-private.h"
#include "gtksourcemark.h"
#include "gtksourcemarkssequence.h"
#include "gtksourcesearchcontext.h"
#include "gtksourceview-i18n.h"
#include "gtksourceview-typebuiltins.h"

/**
 * SECTION:buffer
 * @Short_description: Buffer object for GtkSourceView
 * @Title: GtkSourceBuffer
 * @See_also: #GtkTextBuffer, #GtkSourceView
 *
 * The #GtkSourceBuffer object is the model for #GtkSourceView widgets.
 * It extends the #GtkTextBuffer object by adding features useful to display
 * and edit source code such as syntax highlighting and bracket matching. It
 * also implements support for undo/redo operations, and for the search and
 * replace.
 *
 * To create a #GtkSourceBuffer use gtk_source_buffer_new() or
 * gtk_source_buffer_new_with_language(). The second form is just a convenience
 * function which allows you to initially set a #GtkSourceLanguage.
 *
 * By default highlighting is enabled, but you can disable it with
 * gtk_source_buffer_set_highlight_syntax().
 *
 * # Undo and Redo
 *
 * A custom #GtkSourceUndoManager can be implemented and set with
 * gtk_source_buffer_set_undo_manager(). However the default implementation
 * should be suitable for most uses. By default, actions that can be undone or
 * redone are defined as groups of operations between a call to
 * gtk_text_buffer_begin_user_action() and gtk_text_buffer_end_user_action(). In
 * general, this happens whenever the user presses any key which modifies the
 * buffer. But the default undo manager will try to merge similar consecutive
 * actions, such as multiple character insertions on the same line, into one
 * action. But, inserting a newline starts a new action.
 *
 * The default undo manager remembers the "modified" state of the buffer, and
 * restore it when an action is undone or redone. It can be useful in a text
 * editor to know whether the file is saved. See gtk_text_buffer_get_modified()
 * and gtk_text_buffer_set_modified().
 *
 * # Context Classes
 *
 * It is possible to retrieve some information from the syntax highlighting
 * engine. There are currently three default context classes that are
 * applied to regions of a #GtkSourceBuffer:
 *  - <emphasis>comment</emphasis>: the region delimits a comment;
 *  - <emphasis>string</emphasis>: the region delimits a string;
 *  - <emphasis>no-spell-check</emphasis>: the region should not be spell checked.
 *
 * Custom language definition files can create their own context classes,
 * since the functions like gtk_source_buffer_iter_has_context_class() take
 * a string parameter as the context class.
 */

/*
#define ENABLE_DEBUG
#define ENABLE_PROFILE
*/
#undef ENABLE_DEBUG
#undef ENABLE_PROFILE

#ifdef ENABLE_DEBUG
#define DEBUG(x) (x)
#else
#define DEBUG(x)
#endif

#ifdef ENABLE_PROFILE
#define PROFILE(x) (x)
#else
#define PROFILE(x)
#endif

/* For bracket matching */
#define MAX_CHARS_BEFORE_FINDING_A_MATCH    10000

#define TAG_CONTEXT_CLASS_NAME "GtkSourceViewTagContextClassName"

/* Signals */
enum {
	HIGHLIGHT_UPDATED,
	SOURCE_MARK_UPDATED,
	UNDO,
	REDO,
	BRACKET_MATCHED,
	LAST_SIGNAL
};

/* Properties */
enum {
	PROP_0,
	PROP_CAN_UNDO,
	PROP_CAN_REDO,
	PROP_HIGHLIGHT_SYNTAX,
	PROP_HIGHLIGHT_MATCHING_BRACKETS,
	PROP_MAX_UNDO_LEVELS,
	PROP_LANGUAGE,
	PROP_STYLE_SCHEME,
	PROP_UNDO_MANAGER,
	PROP_IMPLICIT_TRAILING_NEWLINE
};

struct _GtkSourceBufferPrivate
{
	GtkTextTag            *bracket_match_tag;
	GtkTextMark           *bracket_mark_cursor;
	GtkTextMark           *bracket_mark_match;
	GtkSourceBracketMatchType bracket_match;

	/* Hash table: category -> MarksSequence */
	GHashTable             *source_marks;
	GtkSourceMarksSequence *all_source_marks;

	GtkSourceLanguage     *language;

	GtkSourceEngine       *highlight_engine;
	GtkSourceStyleScheme  *style_scheme;

	GtkSourceUndoManager  *undo_manager;
	gint                   max_undo_levels;

	GList                 *search_contexts;

	GtkTextTag            *invalid_char_tag;

	guint                  highlight_syntax : 1;
	guint                  highlight_brackets : 1;
	guint                  constructed : 1;
	guint                  allow_bracket_match : 1;
	guint                  implicit_trailing_newline : 1;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkSourceBuffer, gtk_source_buffer, GTK_TYPE_TEXT_BUFFER)

static guint 	 buffer_signals[LAST_SIGNAL];

static void 	 gtk_source_buffer_dispose		(GObject                 *object);
static void      gtk_source_buffer_set_property         (GObject                 *object,
							 guint                    prop_id,
							 const GValue            *value,
							 GParamSpec              *pspec);
static void      gtk_source_buffer_get_property         (GObject                 *object,
							 guint                    prop_id,
							 GValue                  *value,
							 GParamSpec              *pspec);
static void 	 gtk_source_buffer_can_undo_handler 	(GtkSourceUndoManager    *manager,
							 GtkSourceBuffer         *buffer);
static void 	 gtk_source_buffer_can_redo_handler	(GtkSourceUndoManager    *manager,
							 GtkSourceBuffer         *buffer);
static void 	 gtk_source_buffer_real_insert_text 	(GtkTextBuffer           *buffer,
							 GtkTextIter             *iter,
							 const gchar             *text,
							 gint                     len);
static void	 gtk_source_buffer_real_insert_pixbuf	(GtkTextBuffer           *buffer,
							 GtkTextIter             *pos,
							 GdkPixbuf               *pixbuf);
static void	 gtk_source_buffer_real_insert_anchor	(GtkTextBuffer           *buffer,
							 GtkTextIter             *pos,
							 GtkTextChildAnchor      *anchor);
static void 	 gtk_source_buffer_real_delete_range 	(GtkTextBuffer           *buffer,
							 GtkTextIter             *iter,
							 GtkTextIter             *end);
static void 	 gtk_source_buffer_real_mark_set	(GtkTextBuffer		 *buffer,
							 const GtkTextIter	 *location,
							 GtkTextMark		 *mark);

static void 	 gtk_source_buffer_real_apply_tag	(GtkTextBuffer		 *buffer,
							 GtkTextTag		 *tag,
							 const GtkTextIter	 *start,
							 const GtkTextIter	 *end);

static void 	 gtk_source_buffer_real_mark_deleted	(GtkTextBuffer		 *buffer,
							 GtkTextMark		 *mark);
static gboolean	 gtk_source_buffer_find_bracket_match_with_limit (GtkSourceBuffer *buffer,
								  GtkTextIter     *orig,
								  GtkSourceBracketMatchType *result,
								  gint             max_chars);

static void	 gtk_source_buffer_real_undo		(GtkSourceBuffer	 *buffer);
static void	 gtk_source_buffer_real_redo		(GtkSourceBuffer	 *buffer);

static void
gtk_source_buffer_constructed (GObject *object)
{
	GtkSourceBuffer *buffer = GTK_SOURCE_BUFFER (object);

	/* we need to know that the tag-table was set */
	buffer->priv->constructed = TRUE;

	if (buffer->priv->undo_manager == NULL)
	{
		/* This will install the default undo manager */
		gtk_source_buffer_set_undo_manager (buffer, NULL);
	}

	G_OBJECT_CLASS (gtk_source_buffer_parent_class)->constructed (object);
}

static void
gtk_source_buffer_class_init (GtkSourceBufferClass *klass)
{
	GObjectClass        *object_class;
	GtkTextBufferClass  *tb_class;
	GType                param_types[2];

	object_class 	= G_OBJECT_CLASS (klass);
	tb_class	= GTK_TEXT_BUFFER_CLASS (klass);

	object_class->constructed  = gtk_source_buffer_constructed;
	object_class->dispose	   = gtk_source_buffer_dispose;
	object_class->get_property = gtk_source_buffer_get_property;
	object_class->set_property = gtk_source_buffer_set_property;

	tb_class->delete_range        = gtk_source_buffer_real_delete_range;
	tb_class->insert_text 	      = gtk_source_buffer_real_insert_text;
	tb_class->insert_pixbuf       = gtk_source_buffer_real_insert_pixbuf;
	tb_class->insert_child_anchor = gtk_source_buffer_real_insert_anchor;
	tb_class->apply_tag           = gtk_source_buffer_real_apply_tag;

	tb_class->mark_set	= gtk_source_buffer_real_mark_set;
	tb_class->mark_deleted	= gtk_source_buffer_real_mark_deleted;

	klass->undo = gtk_source_buffer_real_undo;
	klass->redo = gtk_source_buffer_real_redo;

	/**
	 * GtkSourceBuffer:highlight-syntax:
	 *
	 * Whether to highlight syntax in the buffer.
	 */
	g_object_class_install_property (object_class,
					 PROP_HIGHLIGHT_SYNTAX,
					 g_param_spec_boolean ("highlight-syntax",
							       _("Highlight Syntax"),
							       _("Whether to highlight syntax "
								 "in the buffer"),
							       TRUE,
							       G_PARAM_READWRITE));

	/**
	 * GtkSourceBuffer:highlight-matching-brackets:
	 *
	 * Whether to highlight matching brackets in the buffer.
	 */
	g_object_class_install_property (object_class,
					 PROP_HIGHLIGHT_MATCHING_BRACKETS,
					 g_param_spec_boolean ("highlight-matching-brackets",
							       _("Highlight Matching Brackets"),
							       _("Whether to highlight matching brackets"),
							       TRUE,
							       G_PARAM_READWRITE));

	/**
	 * GtkSourceBuffer:max-undo-levels:
	 *
	 * Number of undo levels for the buffer. -1 means no limit. This property
	 * will only affect the default undo manager.
	 */
	g_object_class_install_property (object_class,
					 PROP_MAX_UNDO_LEVELS,
					 g_param_spec_int ("max-undo-levels",
							   _("Maximum Undo Levels"),
							   _("Number of undo levels for "
							     "the buffer"),
							   -1,
							   G_MAXINT,
							   -1,
							   G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_LANGUAGE,
					 g_param_spec_object ("language",
							      /* Translators: throughout GtkSourceView "language" stands
							       * for "programming language", not "spoken language" */
							      _("Language"),
							      _("Language object to get "
								"highlighting patterns from"),
							      GTK_SOURCE_TYPE_LANGUAGE,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_CAN_UNDO,
					 g_param_spec_boolean ("can-undo",
							       _("Can undo"),
							       _("Whether Undo operation is possible"),
							       FALSE,
							       G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_CAN_REDO,
					 g_param_spec_boolean ("can-redo",
							       _("Can redo"),
							       _("Whether Redo operation is possible"),
							       FALSE,
							       G_PARAM_READABLE));

	/**
	 * GtkSourceBuffer:style-scheme:
	 *
	 * Style scheme. It contains styles for syntax highlighting, optionally
	 * foreground, background, cursor color, current line color, and matching
	 * brackets style.
	 */
	g_object_class_install_property (object_class,
					 PROP_STYLE_SCHEME,
					 g_param_spec_object ("style_scheme",
							      _("Style scheme"),
							      _("Style scheme"),
							      GTK_SOURCE_TYPE_STYLE_SCHEME,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
	                                 PROP_UNDO_MANAGER,
	                                 g_param_spec_object ("undo-manager",
	                                                      _("Undo manager"),
	                                                      _("The buffer undo manager"),
	                                                      GTK_SOURCE_TYPE_UNDO_MANAGER,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	/**
	 * GtkSourceBuffer:implicit-trailing-newline:
	 *
	 * Whether the buffer has an implicit trailing newline. See
	 * gtk_source_buffer_set_implicit_trailing_newline().
	 *
	 * Since: 3.14
	 */
	g_object_class_install_property (object_class,
					 PROP_IMPLICIT_TRAILING_NEWLINE,
					 g_param_spec_boolean ("implicit-trailing-newline",
							       _("Implicit trailing newline"),
							       "",
							       TRUE,
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
							       G_PARAM_STATIC_STRINGS));

	param_types[0] = GTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE;
	param_types[1] = GTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE;

	/**
	 * GtkSourceBuffer::highlight-updated:
	 * @buffer: the buffer that received the signal
	 * @start: the start of the updated region
	 * @end: the end of the updated region
	 *
	 * The ::highlight-updated signal is emitted when the syntax
	 * highlighting is updated in a certain region of the @buffer. This
	 * signal is useful to be notified when a context class region is
	 * updated (e.g. the no-spell-check context class).
	 */
	buffer_signals[HIGHLIGHT_UPDATED] =
	    g_signal_newv ("highlight_updated",
			   G_OBJECT_CLASS_TYPE (object_class),
			   G_SIGNAL_RUN_LAST,
			   NULL, NULL, NULL, NULL,
			   G_TYPE_NONE,
			   2, param_types);

	/**
	 * GtkSourceBuffer::source-mark-updated:
	 * @buffer: the buffer that received the signal
	 * @mark: the #GtkSourceMark
	 *
	 * The ::source_mark_updated signal is emitted each time
	 * a mark is added to, moved or removed from the @buffer.
	 **/
	buffer_signals[SOURCE_MARK_UPDATED] =
	    g_signal_new ("source_mark_updated",
			   G_OBJECT_CLASS_TYPE (object_class),
			   G_SIGNAL_RUN_LAST,
			   0,
			   NULL, NULL, NULL,
			   G_TYPE_NONE,
			   1, GTK_TYPE_TEXT_MARK);

	/**
	 * GtkSourceBuffer::undo:
	 * @buffer: the buffer that received the signal
	 *
	 * The ::undo signal is emitted to undo the last user action which
	 * modified the buffer.
	 */
	buffer_signals[UNDO] =
	    g_signal_new ("undo",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (GtkSourceBufferClass, undo),
			  NULL, NULL, NULL,
			  G_TYPE_NONE, 0);

	/**
	 * GtkSourceBuffer::redo:
	 * @buffer: the buffer that received the signal
	 *
	 * The ::redo signal is emitted to redo the last undo operation.
	 */
	buffer_signals[REDO] =
	    g_signal_new ("redo",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (GtkSourceBufferClass, redo),
			  NULL, NULL, NULL,
			  G_TYPE_NONE, 0);

	/**
	 * GtkSourceBuffer::bracket-matched:
	 * @buffer: a #GtkSourceBuffer.
	 * @iter: iterator to initialize.
	 * @state: state of bracket matching
	 *
	 * Sets @iter to a valid iterator pointing to the matching bracket
	 * if @state is #GTK_SOURCE_BRACKET_MATCH_FOUND. Otherwise @iter is
	 * meaningless.
	 *
	 * Since: 2.12
	 */
	buffer_signals[BRACKET_MATCHED] =
	    g_signal_new ("bracket-matched",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (GtkSourceBufferClass, bracket_matched),
			  NULL, NULL, NULL,
			  G_TYPE_NONE, 2,
			  GTK_TYPE_TEXT_ITER,
			  GTK_SOURCE_TYPE_BRACKET_MATCH_TYPE);
}

static void
set_undo_manager (GtkSourceBuffer      *buffer,
                  GtkSourceUndoManager *manager)
{
	if (manager == buffer->priv->undo_manager)
	{
		return;
	}

	if (buffer->priv->undo_manager != NULL)
	{
		g_signal_handlers_disconnect_by_func (buffer->priv->undo_manager,
		                                      G_CALLBACK (gtk_source_buffer_can_undo_handler),
		                                      buffer);

		g_signal_handlers_disconnect_by_func (buffer->priv->undo_manager,
		                                      G_CALLBACK (gtk_source_buffer_can_redo_handler),
		                                      buffer);

		g_object_unref (buffer->priv->undo_manager);
		buffer->priv->undo_manager = NULL;
	}

	if (manager != NULL)
	{
		buffer->priv->undo_manager = g_object_ref (manager);

		g_signal_connect (buffer->priv->undo_manager,
		                  "can-undo-changed",
		                  G_CALLBACK (gtk_source_buffer_can_undo_handler),
		                  buffer);

		g_signal_connect (buffer->priv->undo_manager,
		                  "can-redo-changed",
		                  G_CALLBACK (gtk_source_buffer_can_redo_handler),
		                  buffer);

		/* Notify possible changes in the can-undo/redo state */
		g_object_notify (G_OBJECT (buffer), "can-undo");
		g_object_notify (G_OBJECT (buffer), "can-redo");
	}
}

static void
search_context_weak_notify_cb (GtkSourceBuffer *buffer,
			       GObject         *search_context)
{
	buffer->priv->search_contexts = g_list_remove (buffer->priv->search_contexts,
						       search_context);
}

static void
gtk_source_buffer_init (GtkSourceBuffer *buffer)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	buffer->priv = priv;

	priv->highlight_syntax = TRUE;
	priv->highlight_brackets = TRUE;
	priv->bracket_mark_cursor = NULL;
	priv->bracket_mark_match = NULL;
	priv->bracket_match = GTK_SOURCE_BRACKET_MATCH_NONE;
	priv->max_undo_levels = -1;

	priv->source_marks = g_hash_table_new_full (g_str_hash,
						    g_str_equal,
						    (GDestroyNotify)g_free,
						    (GDestroyNotify)g_object_unref);

	priv->all_source_marks = _gtk_source_marks_sequence_new (GTK_TEXT_BUFFER (buffer));

	priv->style_scheme = _gtk_source_style_scheme_get_default ();

	if (priv->style_scheme != NULL)
	{
		g_object_ref (priv->style_scheme);
	}
}

static void
gtk_source_buffer_dispose (GObject *object)
{
	GtkSourceBuffer *buffer;
	GList *l;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GTK_SOURCE_IS_BUFFER (object));

	buffer = GTK_SOURCE_BUFFER (object);

	if (buffer->priv->undo_manager != NULL)
	{
		set_undo_manager (buffer, NULL);
	}

	if (buffer->priv->highlight_engine != NULL)
	{
		_gtk_source_engine_attach_buffer (buffer->priv->highlight_engine, NULL);
	}

	g_clear_object (&buffer->priv->highlight_engine);
	g_clear_object (&buffer->priv->language);
	g_clear_object (&buffer->priv->style_scheme);

	for (l = buffer->priv->search_contexts; l != NULL; l = l->next)
	{
		GtkSourceSearchContext *search_context = l->data;

		g_object_weak_unref (G_OBJECT (search_context),
				     (GWeakNotify)search_context_weak_notify_cb,
				     buffer);
	}

	g_list_free (buffer->priv->search_contexts);
	buffer->priv->search_contexts = NULL;

	g_clear_object (&buffer->priv->all_source_marks);

	if (buffer->priv->source_marks != NULL)
	{
		g_hash_table_unref (buffer->priv->source_marks);
		buffer->priv->source_marks = NULL;
	}

	G_OBJECT_CLASS (gtk_source_buffer_parent_class)->dispose (object);
}

static void
gtk_source_buffer_set_property (GObject      *object,
				guint         prop_id,
				const GValue *value,
				GParamSpec   *pspec)
{
	GtkSourceBuffer *source_buffer;

	g_return_if_fail (GTK_SOURCE_IS_BUFFER (object));

	source_buffer = GTK_SOURCE_BUFFER (object);

	switch (prop_id)
	{
		case PROP_HIGHLIGHT_SYNTAX:
			gtk_source_buffer_set_highlight_syntax (source_buffer,
								g_value_get_boolean (value));
			break;

		case PROP_HIGHLIGHT_MATCHING_BRACKETS:
			gtk_source_buffer_set_highlight_matching_brackets (source_buffer,
									   g_value_get_boolean (value));
			break;

		case PROP_MAX_UNDO_LEVELS:
			gtk_source_buffer_set_max_undo_levels (source_buffer,
							       g_value_get_int (value));
			break;

		case PROP_LANGUAGE:
			gtk_source_buffer_set_language (source_buffer,
							g_value_get_object (value));
			break;

		case PROP_STYLE_SCHEME:
			gtk_source_buffer_set_style_scheme (source_buffer,
							    g_value_get_object (value));
			break;

		case PROP_UNDO_MANAGER:
			gtk_source_buffer_set_undo_manager (source_buffer,
			                                    g_value_get_object (value));
			break;

		case PROP_IMPLICIT_TRAILING_NEWLINE:
			gtk_source_buffer_set_implicit_trailing_newline (source_buffer,
									 g_value_get_boolean (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_buffer_get_property (GObject    *object,
				guint       prop_id,
				GValue     *value,
				GParamSpec *pspec)
{
	GtkSourceBuffer *source_buffer;

	g_return_if_fail (GTK_SOURCE_IS_BUFFER (object));

	source_buffer = GTK_SOURCE_BUFFER (object);

	switch (prop_id)
	{
		case PROP_HIGHLIGHT_SYNTAX:
			g_value_set_boolean (value,
					     source_buffer->priv->highlight_syntax);
			break;

		case PROP_HIGHLIGHT_MATCHING_BRACKETS:
			g_value_set_boolean (value,
					     source_buffer->priv->highlight_brackets);
			break;

		case PROP_MAX_UNDO_LEVELS:
			g_value_set_int (value,
					 source_buffer->priv->max_undo_levels);
			break;

		case PROP_LANGUAGE:
			g_value_set_object (value, source_buffer->priv->language);
			break;

		case PROP_STYLE_SCHEME:
			g_value_set_object (value, source_buffer->priv->style_scheme);
			break;

		case PROP_CAN_UNDO:
			g_value_set_boolean (value, gtk_source_buffer_can_undo (source_buffer));
			break;

		case PROP_CAN_REDO:
			g_value_set_boolean (value, gtk_source_buffer_can_redo (source_buffer));
			break;

		case PROP_UNDO_MANAGER:
			g_value_set_object (value, source_buffer->priv->undo_manager);
			break;

		case PROP_IMPLICIT_TRAILING_NEWLINE:
			g_value_set_boolean (value, source_buffer->priv->implicit_trailing_newline);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

/**
 * gtk_source_buffer_new:
 * @table: (allow-none): a #GtkTextTagTable, or %NULL to create a new one.
 *
 * Creates a new source buffer.
 *
 * Return value: a new source buffer.
 **/
GtkSourceBuffer *
gtk_source_buffer_new (GtkTextTagTable *table)
{
	return g_object_new (GTK_SOURCE_TYPE_BUFFER,
			     "tag-table", table,
			     NULL);
}

/**
 * gtk_source_buffer_new_with_language:
 * @language: a #GtkSourceLanguage.
 *
 * Creates a new source buffer using the highlighting patterns in
 * @language.  This is equivalent to creating a new source buffer with
 * a new tag table and then calling gtk_source_buffer_set_language().
 *
 * Return value: a new source buffer which will highlight text
 * according to the highlighting patterns in @language.
 **/
GtkSourceBuffer *
gtk_source_buffer_new_with_language (GtkSourceLanguage *language)
{
	g_return_val_if_fail (GTK_SOURCE_IS_LANGUAGE (language), NULL);

	return g_object_new (GTK_SOURCE_TYPE_BUFFER,
			     "tag-table", NULL,
			     "language", language,
			     NULL);
}

static void
gtk_source_buffer_can_undo_handler (GtkSourceUndoManager *manager,
                                    GtkSourceBuffer      *buffer)
{
	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));

	g_object_notify (G_OBJECT (buffer), "can-undo");
}

static void
gtk_source_buffer_can_redo_handler (GtkSourceUndoManager *manager,
                                    GtkSourceBuffer      *buffer)
{
	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));

	g_object_notify (G_OBJECT (buffer), "can-redo");
}

static void
update_bracket_match_style (GtkSourceBuffer *buffer)
{
	if (buffer->priv->bracket_match_tag != NULL)
	{
		GtkSourceStyle *style = NULL;

		if (buffer->priv->style_scheme)
			style = _gtk_source_style_scheme_get_matching_brackets_style (buffer->priv->style_scheme);

		_gtk_source_style_apply (style, buffer->priv->bracket_match_tag);
	}
}

static GtkTextTag *
get_bracket_match_tag (GtkSourceBuffer *buffer)
{
	if (buffer->priv->bracket_match_tag == NULL)
	{
		buffer->priv->bracket_match_tag =
			gtk_text_buffer_create_tag (GTK_TEXT_BUFFER (buffer),
						    NULL,
						    NULL);
		update_bracket_match_style (buffer);
	}

	return buffer->priv->bracket_match_tag;
}

/*
 * This is private, just used by the compositor to not print bracket
 * matches. Note that unlike get_bracket_match_tag() it returns NULL
 * if the tag is not set.
 */
GtkTextTag *
_gtk_source_buffer_get_bracket_match_tag (GtkSourceBuffer *buffer)
{
	return buffer->priv->bracket_match_tag;
}

static gunichar
bracket_pair (gunichar base_char, gint *direction)
{
	gint dir;
	gunichar pair;

	switch ((int)base_char)
	{
		case '{':
			dir = 1;
			pair = '}';
			break;
		case '(':
			dir = 1;
			pair = ')';
			break;
		case '[':
			dir = 1;
			pair = ']';
			break;
		case '<':
			dir = 1;
			pair = '>';
			break;
		case '}':
			dir = -1;
			pair = '{';
			break;
		case ')':
			dir = -1;
			pair = '(';
			break;
		case ']':
			dir = -1;
			pair = '[';
			break;
		case '>':
			dir = -1;
			pair = '<';
			break;
		default:
			dir = 0;
			pair = 0;
			break;
	}

	/* Let direction be NULL if we don't care */
	if (direction != NULL)
		*direction = dir;

	return pair;
}

static void
gtk_source_buffer_move_cursor (GtkTextBuffer     *buffer,
			       const GtkTextIter *iter,
			       GtkTextMark       *mark)
{
	GtkSourceBuffer *source_buffer;
	GtkTextIter start, end;
	gunichar cursor_char;
	GtkSourceBracketMatchType previous_state;

	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));
	g_return_if_fail (iter != NULL);
	g_return_if_fail (mark != NULL);
	g_return_if_fail (gtk_text_iter_get_buffer (iter) == buffer);

	if (mark != gtk_text_buffer_get_insert (buffer))
		return;

	source_buffer = GTK_SOURCE_BUFFER (buffer);

	if (source_buffer->priv->bracket_match == GTK_SOURCE_BRACKET_MATCH_FOUND)
	{
		gtk_text_buffer_get_iter_at_mark (buffer,
						  &start,
						  source_buffer->priv->bracket_mark_match);

		gtk_text_buffer_get_iter_at_mark (buffer,
						  &end,
						  source_buffer->priv->bracket_mark_cursor);

		gtk_text_iter_order (&start, &end);
		gtk_text_iter_forward_char (&end);
		gtk_text_buffer_remove_tag (buffer,
					    source_buffer->priv->bracket_match_tag,
					    &start,
					    &end);
	}

	if (!source_buffer->priv->highlight_brackets)
		return;

	start = *iter;
	previous_state = source_buffer->priv->bracket_match;
	if (!gtk_source_buffer_find_bracket_match_with_limit (source_buffer,
	                                                      &start,
	                                                      &source_buffer->priv->bracket_match,
	                                                      MAX_CHARS_BEFORE_FINDING_A_MATCH))
	{
		/* don't emit the signal at all if chars at previous and current
		   positions are nonbrackets. */
		if (previous_state != GTK_SOURCE_BRACKET_MATCH_NONE ||
		    source_buffer->priv->bracket_match != GTK_SOURCE_BRACKET_MATCH_NONE)
		{
			g_signal_emit (source_buffer,
				       buffer_signals[BRACKET_MATCHED],
				       0,
				       &end,
				       source_buffer->priv->bracket_match);
		}
	}
	else
	{
		g_signal_emit (source_buffer,
			       buffer_signals[BRACKET_MATCHED],
			       0,
			       &start,
			       source_buffer->priv->bracket_match);

		/* allow_bracket_match will allow the bracket match tag to be
		   applied to the buffer. See apply_tag_real for more
		   information */
		source_buffer->priv->allow_bracket_match = TRUE;

		/* Mark matching bracket */
		if (!source_buffer->priv->bracket_mark_match)
		{
			source_buffer->priv->bracket_mark_match =
 				gtk_text_buffer_create_mark (buffer,
							     NULL,
							     &start,
							     TRUE);
 		}
 		else
 		{
 			gtk_text_buffer_move_mark (buffer,
						   source_buffer->priv->bracket_mark_match,
						   &start);
 		}

		end = start;
		gtk_text_iter_forward_char (&end);
		gtk_text_buffer_apply_tag (buffer,
					   get_bracket_match_tag (source_buffer),
					   &start,
					   &end);

		/* Mark the bracket near the cursor */
		start = *iter;
		cursor_char = gtk_text_iter_get_char (&start);
		if (bracket_pair (cursor_char, NULL) == 0)
			gtk_text_iter_backward_char (&start);

		if (!source_buffer->priv->bracket_mark_cursor)
		{
			source_buffer->priv->bracket_mark_cursor =
				gtk_text_buffer_create_mark (buffer,
							     NULL,
							     &start,
							     FALSE);
		}
		else
		{
			gtk_text_buffer_move_mark (buffer,
						   source_buffer->priv->bracket_mark_cursor,
						   &start);
		}

		end = start;
		gtk_text_iter_forward_char (&end);
		gtk_text_buffer_apply_tag (buffer,
					   get_bracket_match_tag (source_buffer),
					   &start,
					   &end);

		source_buffer->priv->allow_bracket_match = FALSE;
	}
}

static void
gtk_source_buffer_content_inserted (GtkTextBuffer *buffer,
				    gint           start_offset,
				    gint           end_offset)
{
	GtkTextMark *mark;
	GtkTextIter insert_iter;
	GtkSourceBuffer *source_buffer = GTK_SOURCE_BUFFER (buffer);

	mark = gtk_text_buffer_get_insert (buffer);
	gtk_text_buffer_get_iter_at_mark (buffer, &insert_iter, mark);
	gtk_source_buffer_move_cursor (buffer, &insert_iter, mark);

	if (source_buffer->priv->highlight_engine != NULL)
		_gtk_source_engine_text_inserted (source_buffer->priv->highlight_engine,
						  start_offset,
						  end_offset);
}

static void
gtk_source_buffer_real_insert_text (GtkTextBuffer *buffer,
				    GtkTextIter   *iter,
				    const gchar   *text,
				    gint           len)
{
	gint start_offset;

	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));
	g_return_if_fail (iter != NULL);
	g_return_if_fail (text != NULL);
	g_return_if_fail (gtk_text_iter_get_buffer (iter) == buffer);

	start_offset = gtk_text_iter_get_offset (iter);

	/*
	 * iter is invalidated when
	 * insertion occurs (because the buffer contents change), but the
	 * default signal handler revalidates it to point to the end of the
	 * inserted text
	 */
	GTK_TEXT_BUFFER_CLASS (gtk_source_buffer_parent_class)->insert_text (buffer, iter, text, len);

	gtk_source_buffer_content_inserted (buffer,
					    start_offset,
					    gtk_text_iter_get_offset (iter));
}

/* insert_pixbuf and insert_child_anchor do nothing except notifying
 * the highlighting engine about the change, because engine's idea
 * of buffer char count must be correct at all times */
static void
gtk_source_buffer_real_insert_pixbuf (GtkTextBuffer *buffer,
				      GtkTextIter   *iter,
				      GdkPixbuf     *pixbuf)
{
	gint start_offset;

	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));
	g_return_if_fail (iter != NULL);
	g_return_if_fail (gtk_text_iter_get_buffer (iter) == buffer);

	start_offset = gtk_text_iter_get_offset (iter);

	/*
	 * iter is invalidated when
	 * insertion occurs (because the buffer contents change), but the
	 * default signal handler revalidates it to point to the end of the
	 * inserted text
	 */
	GTK_TEXT_BUFFER_CLASS (gtk_source_buffer_parent_class)->insert_pixbuf (buffer, iter, pixbuf);

	gtk_source_buffer_content_inserted (buffer,
					    start_offset,
					    gtk_text_iter_get_offset (iter));
}

static void
gtk_source_buffer_real_insert_anchor (GtkTextBuffer      *buffer,
				      GtkTextIter        *iter,
				      GtkTextChildAnchor *anchor)
{
	gint start_offset;

	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));
	g_return_if_fail (iter != NULL);
	g_return_if_fail (gtk_text_iter_get_buffer (iter) == buffer);

	start_offset = gtk_text_iter_get_offset (iter);

	/*
	 * iter is invalidated when
	 * insertion occurs (because the buffer contents change), but the
	 * default signal handler revalidates it to point to the end of the
	 * inserted text
	 */
	GTK_TEXT_BUFFER_CLASS (gtk_source_buffer_parent_class)->insert_child_anchor (buffer, iter, anchor);

	gtk_source_buffer_content_inserted (buffer,
					    start_offset,
					    gtk_text_iter_get_offset (iter));
}

static void
gtk_source_buffer_real_delete_range (GtkTextBuffer *buffer,
				     GtkTextIter   *start,
				     GtkTextIter   *end)
{
	gint offset, length;
	GtkTextMark *mark;
	GtkTextIter iter;
	GtkSourceBuffer *source_buffer = GTK_SOURCE_BUFFER (buffer);

	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));
	g_return_if_fail (start != NULL);
	g_return_if_fail (end != NULL);
	g_return_if_fail (gtk_text_iter_get_buffer (start) == buffer);
	g_return_if_fail (gtk_text_iter_get_buffer (end) == buffer);

	gtk_text_iter_order (start, end);
	offset = gtk_text_iter_get_offset (start);
	length = gtk_text_iter_get_offset (end) - offset;

	GTK_TEXT_BUFFER_CLASS (gtk_source_buffer_parent_class)->delete_range (buffer, start, end);

	mark = gtk_text_buffer_get_insert (buffer);
	gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);
	gtk_source_buffer_move_cursor (buffer, &iter, mark);

	/* emit text deleted for engines */
	if (source_buffer->priv->highlight_engine != NULL)
		_gtk_source_engine_text_deleted (source_buffer->priv->highlight_engine,
						 offset, length);
}

/* This describes a mask of relevant context classes for highlighting matching
   brackets. Additional classes can be added below */
static const gchar *cclass_mask_definitions[] = {
	"comment",
	"string",
};

static gint
get_context_class_mask (GtkSourceBuffer *buffer,
                        GtkTextIter     *iter)
{
	gint i;
	gint ret = 0;

	for (i = 0; i < sizeof (cclass_mask_definitions) / sizeof (gchar *); ++i)
	{
		gboolean hasclass = gtk_source_buffer_iter_has_context_class (buffer,
		                                                              iter,
		                                                              cclass_mask_definitions[i]);

		ret |= hasclass << i;
	}

	return ret;
}

static gboolean
gtk_source_buffer_find_bracket_match_real (GtkSourceBuffer           *buffer,
                                           GtkTextIter               *orig,
					   GtkSourceBracketMatchType *result,
                                           gint                       max_chars)
{
	GtkTextIter iter;

	gunichar base_char;
	gunichar search_char;
	gunichar cur_char;
	gint addition;
	gint char_cont;
	gint counter;

	gboolean found;

	gint cclass_mask;

	iter = *orig;

	cur_char = gtk_text_iter_get_char (&iter);

	base_char = cur_char;
	cclass_mask = get_context_class_mask (buffer, &iter);

	search_char = bracket_pair (base_char, &addition);

	if (addition == 0)
	{
		*result = GTK_SOURCE_BRACKET_MATCH_NONE;
		return FALSE;
	}

	counter = 0;
	found = FALSE;
	char_cont = 0;

	do
	{
		gint current_mask;

		gtk_text_iter_forward_chars (&iter, addition);
		cur_char = gtk_text_iter_get_char (&iter);
		++char_cont;

		current_mask = get_context_class_mask (buffer, &iter);

		/* Check if we lost a class, which means we don't look any
		   further */
		if (current_mask < cclass_mask)
		{
			found = FALSE;
			break;
		}

		if ((cur_char == search_char || cur_char == base_char) &&
		    cclass_mask == current_mask)
		{
			if ((cur_char == search_char) && counter == 0)
			{
				found = TRUE;
				break;
			}
			if (cur_char == base_char)
				counter++;
			else
				counter--;
		}
	}
	while (!gtk_text_iter_is_end (&iter) && !gtk_text_iter_is_start (&iter) &&
		((char_cont < max_chars) || (max_chars < 0)));

	if (found)
	{
		*orig = iter;
		*result = GTK_SOURCE_BRACKET_MATCH_FOUND;
	}
	else if (char_cont >= max_chars && max_chars >= 0)
	{
		*result = GTK_SOURCE_BRACKET_MATCH_OUT_OF_RANGE;
	}
	else
	{
		*result = GTK_SOURCE_BRACKET_MATCH_NOT_FOUND;
	}

	return found;
}

/* Note that we take into account both the character following the cursor and the
 * one preceding it. If there are brackets on both sides the one following the
 * cursor takes precedence.
 */
static gboolean
gtk_source_buffer_find_bracket_match_with_limit (GtkSourceBuffer           *buffer,
                                                 GtkTextIter               *orig,
                                                 GtkSourceBracketMatchType *result,
                                                 gint                       max_chars)
{
	GtkTextIter iter;

	if (gtk_source_buffer_find_bracket_match_real (buffer, orig, result, max_chars))
	{
		return TRUE;
	}

	iter = *orig;
	if (!gtk_text_iter_starts_line (&iter) &&
	    gtk_text_iter_backward_char (&iter))
	{
		if (gtk_source_buffer_find_bracket_match_real (buffer, &iter, result, max_chars))
		{
			*orig = iter;
			return TRUE;
		}
	}

	return FALSE;
}

/**
 * gtk_source_buffer_can_undo:
 * @buffer: a #GtkSourceBuffer.
 *
 * Determines whether a source buffer can undo the last action.
 *
 * Return value: %TRUE if it's possible to undo the last action.
 **/
gboolean
gtk_source_buffer_can_undo (GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), FALSE);

	return gtk_source_undo_manager_can_undo (buffer->priv->undo_manager);
}

/**
 * gtk_source_buffer_can_redo:
 * @buffer: a #GtkSourceBuffer.
 *
 * Determines whether a source buffer can redo the last action
 * (i.e. if the last operation was an undo).
 *
 * Return value: %TRUE if a redo is possible.
 **/
gboolean
gtk_source_buffer_can_redo (GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), FALSE);

	return gtk_source_undo_manager_can_redo (buffer->priv->undo_manager);
}

/**
 * gtk_source_buffer_undo:
 * @buffer: a #GtkSourceBuffer.
 *
 * Undoes the last user action which modified the buffer.  Use
 * gtk_source_buffer_can_undo() to check whether a call to this
 * function will have any effect.
 *
 * This function emits the #GtkSourceBuffer::undo signal.
 **/
void
gtk_source_buffer_undo (GtkSourceBuffer *buffer)
{
	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));

	g_signal_emit (buffer, buffer_signals[UNDO], 0);
}

/**
 * gtk_source_buffer_redo:
 * @buffer: a #GtkSourceBuffer.
 *
 * Redoes the last undo operation.  Use gtk_source_buffer_can_redo()
 * to check whether a call to this function will have any effect.
 *
 * This function emits the #GtkSourceBuffer::redo signal.
 **/
void
gtk_source_buffer_redo (GtkSourceBuffer *buffer)
{
	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));

	g_signal_emit (buffer, buffer_signals[REDO], 0);
}

/**
 * gtk_source_buffer_get_max_undo_levels:
 * @buffer: a #GtkSourceBuffer.
 *
 * Determines the number of undo levels the buffer will track for
 * buffer edits.
 *
 * Return value: the maximum number of possible undo levels or
 *               -1 if no limit is set.
 **/
gint
gtk_source_buffer_get_max_undo_levels (GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), 0);

	return buffer->priv->max_undo_levels;
}

/**
 * gtk_source_buffer_set_max_undo_levels:
 * @buffer: a #GtkSourceBuffer.
 * @max_undo_levels: the desired maximum number of undo levels.
 *
 * Sets the number of undo levels for user actions the buffer will
 * track.  If the number of user actions exceeds the limit set by this
 * function, older actions will be discarded.
 *
 * If @max_undo_levels is -1, no limit is set.
 **/
void
gtk_source_buffer_set_max_undo_levels (GtkSourceBuffer *buffer,
				       gint             max_undo_levels)
{
	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));

	if (buffer->priv->max_undo_levels == max_undo_levels)
	{
		return;
	}

	buffer->priv->max_undo_levels = max_undo_levels;

	if (GTK_SOURCE_IS_UNDO_MANAGER_DEFAULT (buffer->priv->undo_manager))
	{
		gtk_source_undo_manager_default_set_max_undo_levels (GTK_SOURCE_UNDO_MANAGER_DEFAULT (buffer->priv->undo_manager),
		                                                     max_undo_levels);
	}

	g_object_notify (G_OBJECT (buffer), "max-undo-levels");
}

/**
 * gtk_source_buffer_begin_not_undoable_action:
 * @buffer: a #GtkSourceBuffer.
 *
 * Marks the beginning of a not undoable action on the buffer,
 * disabling the undo manager.  Typically you would call this function
 * before initially setting the contents of the buffer (e.g. when
 * loading a file in a text editor).
 *
 * You may nest gtk_source_buffer_begin_not_undoable_action() /
 * gtk_source_buffer_end_not_undoable_action() blocks.
 **/
void
gtk_source_buffer_begin_not_undoable_action (GtkSourceBuffer *buffer)
{
	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));

	gtk_source_undo_manager_begin_not_undoable_action (buffer->priv->undo_manager);
}

/**
 * gtk_source_buffer_end_not_undoable_action:
 * @buffer: a #GtkSourceBuffer.
 *
 * Marks the end of a not undoable action on the buffer.  When the
 * last not undoable block is closed through the call to this
 * function, the list of undo actions is cleared and the undo manager
 * is re-enabled.
 **/
void
gtk_source_buffer_end_not_undoable_action (GtkSourceBuffer *buffer)
{
	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));

	gtk_source_undo_manager_end_not_undoable_action (buffer->priv->undo_manager);
}

/**
 * gtk_source_buffer_get_highlight_matching_brackets:
 * @buffer: a #GtkSourceBuffer.
 *
 * Determines whether bracket match highlighting is activated for the
 * source buffer.
 *
 * Return value: %TRUE if the source buffer will highlight matching
 * brackets.
 **/
gboolean
gtk_source_buffer_get_highlight_matching_brackets (GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), FALSE);

	return (buffer->priv->highlight_brackets != FALSE);
}

/**
 * gtk_source_buffer_set_highlight_matching_brackets:
 * @buffer: a #GtkSourceBuffer.
 * @highlight: %TRUE if you want matching brackets highlighted.
 *
 * Controls the bracket match highlighting function in the buffer.  If
 * activated, when you position your cursor over a bracket character
 * (a parenthesis, a square bracket, etc.) the matching opening or
 * closing bracket character will be highlighted.
 **/
void
gtk_source_buffer_set_highlight_matching_brackets (GtkSourceBuffer *buffer,
						   gboolean         highlight)
{
	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));

	highlight = (highlight != FALSE);

	if (highlight != buffer->priv->highlight_brackets)
	{
		GtkTextIter iter;
		GtkTextMark *mark;

		buffer->priv->highlight_brackets = highlight;

		/* try to see if there is already a bracket match at the
		 * current position, but only if the tag table is already set
		 * otherwise we have problems when calling this function
		 * on init (get_insert creates the tag table as a side effect */
		if (buffer->priv->constructed)
		{
			mark = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (buffer));
			gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer), &iter, mark);
			gtk_source_buffer_move_cursor (GTK_TEXT_BUFFER (buffer), &iter, mark);
		}

		g_object_notify (G_OBJECT (buffer), "highlight-matching-brackets");
	}
}

/**
 * gtk_source_buffer_get_highlight_syntax:
 * @buffer: a #GtkSourceBuffer.
 *
 * Determines whether syntax highlighting is activated in the source
 * buffer.
 *
 * Return value: %TRUE if syntax highlighting is enabled, %FALSE otherwise.
 **/
gboolean
gtk_source_buffer_get_highlight_syntax (GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), FALSE);

	return (buffer->priv->highlight_syntax != FALSE);
}

/**
 * gtk_source_buffer_set_highlight_syntax:
 * @buffer: a #GtkSourceBuffer.
 * @highlight: %TRUE to enable syntax highlighting, %FALSE to disable it.
 *
 * Controls whether syntax is highlighted in the buffer. If @highlight
 * is %TRUE, the text will be highlighted according to the syntax
 * patterns specified in the language set with
 * gtk_source_buffer_set_language(). If @highlight is %FALSE, syntax highlighting
 * is disabled and all the GtkTextTag objects that have been added by the
 * syntax highlighting engine are removed from the buffer.
 **/
void
gtk_source_buffer_set_highlight_syntax (GtkSourceBuffer *buffer,
					gboolean         highlight)
{
	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));

	highlight = (highlight != FALSE);

	if (buffer->priv->highlight_syntax != highlight)
	{
		buffer->priv->highlight_syntax = highlight;
		g_object_notify (G_OBJECT (buffer), "highlight-syntax");
	}
}

/**
 * gtk_source_buffer_set_language:
 * @buffer: a #GtkSourceBuffer.
 * @language: (allow-none): a #GtkSourceLanguage to set, or %NULL.
 *
 * Associate a #GtkSourceLanguage with the buffer. If @language is
 * not-%NULL and syntax highlighting is enabled (see gtk_source_buffer_set_highlight_syntax()),
 * the syntax patterns defined in @language will be used to highlight the text
 * contained in the buffer. If @language is %NULL, the text contained in the
 * buffer is not highlighted.
 *
 * The buffer holds a reference to @language.
 **/
void
gtk_source_buffer_set_language (GtkSourceBuffer   *buffer,
				GtkSourceLanguage *language)
{
	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));
	g_return_if_fail (GTK_SOURCE_IS_LANGUAGE (language) || language == NULL);

	if (buffer->priv->language == language)
		return;

	if (buffer->priv->highlight_engine != NULL)
	{
		/* disconnect the old engine */
		_gtk_source_engine_attach_buffer (buffer->priv->highlight_engine, NULL);
		g_object_unref (buffer->priv->highlight_engine);
		buffer->priv->highlight_engine = NULL;
	}

	if (buffer->priv->language != NULL)
		g_object_unref (buffer->priv->language);

	buffer->priv->language = language;

	if (language != NULL)
	{
		g_object_ref (language);

		/* get a new engine */
		buffer->priv->highlight_engine = _gtk_source_language_create_engine (language);

		if (buffer->priv->highlight_engine)
		{
			_gtk_source_engine_attach_buffer (buffer->priv->highlight_engine,
							  GTK_TEXT_BUFFER (buffer));

			if (buffer->priv->style_scheme)
				_gtk_source_engine_set_style_scheme (buffer->priv->highlight_engine,
								     buffer->priv->style_scheme);
		}
	}

	g_object_notify (G_OBJECT (buffer), "language");
}

/**
 * gtk_source_buffer_get_language:
 * @buffer: a #GtkSourceBuffer.
 *
 * Returns the #GtkSourceLanguage associated with the buffer,
 * see gtk_source_buffer_set_language().  The returned object should not be
 * unreferenced by the user.
 *
 * Return value: (transfer none): the #GtkSourceLanguage associated with the buffer, or %NULL.
 **/
GtkSourceLanguage *
gtk_source_buffer_get_language (GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), NULL);

	return buffer->priv->language;
}

/**
 * _gtk_source_buffer_update_highlight:
 * @buffer: a #GtkSourceBuffer.
 * @start: start of the area to highlight.
 * @end: end of the area to highlight.
 * @synchronous: whether the area should be highlighted synchronously.
 *
 * Asks the buffer to analyze and highlight given area.
 **/
void
_gtk_source_buffer_update_highlight (GtkSourceBuffer   *buffer,
				     const GtkTextIter *start,
				     const GtkTextIter *end,
				     gboolean           synchronous)
{
	GList *l;

	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));

	if (buffer->priv->highlight_engine != NULL)
	{
		_gtk_source_engine_update_highlight (buffer->priv->highlight_engine,
						     start,
						     end,
						     synchronous);
	}

	for (l = buffer->priv->search_contexts; l != NULL; l = l->next)
	{
		GtkSourceSearchContext *search_context = l->data;

		_gtk_source_search_context_update_highlight (search_context,
							     start,
							     end,
							     synchronous);
	}
}

/**
 * gtk_source_buffer_ensure_highlight:
 * @buffer: a #GtkSourceBuffer.
 * @start: start of the area to highlight.
 * @end: end of the area to highlight.
 *
 * Forces buffer to analyze and highlight the given area synchronously.
 *
 * <note>
 *   <para>
 *     This is a potentially slow operation and should be used only
 *     when you need to make sure that some text not currently
 *     visible is highlighted, for instance before printing.
 *   </para>
 * </note>
 **/
void
gtk_source_buffer_ensure_highlight (GtkSourceBuffer   *buffer,
				    const GtkTextIter *start,
				    const GtkTextIter *end)
{
	_gtk_source_buffer_update_highlight (buffer,
					     start,
					     end,
					     TRUE);
}

/**
 * gtk_source_buffer_set_style_scheme:
 * @buffer: a #GtkSourceBuffer.
 * @scheme: (allow-none): a #GtkSourceStyleScheme or %NULL.
 *
 * Sets style scheme used by the buffer. If @scheme is %NULL no
 * style scheme is used.
 **/
void
gtk_source_buffer_set_style_scheme (GtkSourceBuffer      *buffer,
				    GtkSourceStyleScheme *scheme)
{
	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));
	g_return_if_fail (GTK_SOURCE_IS_STYLE_SCHEME (scheme) || scheme == NULL);

	if (buffer->priv->style_scheme == scheme)
		return;

	if (buffer->priv->style_scheme)
		g_object_unref (buffer->priv->style_scheme);

	buffer->priv->style_scheme = scheme ? g_object_ref (scheme) : NULL;
	update_bracket_match_style (buffer);

	if (buffer->priv->highlight_engine != NULL)
		_gtk_source_engine_set_style_scheme (buffer->priv->highlight_engine,
						     scheme);

	g_object_notify (G_OBJECT (buffer), "style-scheme");
}

/**
 * gtk_source_buffer_get_style_scheme:
 * @buffer: a #GtkSourceBuffer.
 *
 * Returns the #GtkSourceStyleScheme associated with the buffer,
 * see gtk_source_buffer_set_style_scheme().
 * The returned object should not be unreferenced by the user.
 *
 * Return value: (transfer none): the #GtkSourceStyleScheme associated
 * with the buffer, or %NULL.
 **/
GtkSourceStyleScheme *
gtk_source_buffer_get_style_scheme (GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), NULL);

	return buffer->priv->style_scheme;
}

static void
gtk_source_buffer_real_apply_tag (GtkTextBuffer     *buffer,
                                  GtkTextTag        *tag,
                                  const GtkTextIter *start,
                                  const GtkTextIter *end)
{
	GtkSourceBuffer *source;

	source = GTK_SOURCE_BUFFER (buffer);

	/* We only allow the bracket match tag to be applied when we are doing
	   it ourselves (i.e. when allow_bracket_match is TRUE). The reason for
	   doing so is that when you copy/paste from the same buffer, the tags
	   get pasted too. This is ok for highlighting because the region will
	   get rehighlighted, but not for bracket matching. */
	if (source->priv->allow_bracket_match || tag != get_bracket_match_tag (source))
	{
		GTK_TEXT_BUFFER_CLASS (gtk_source_buffer_parent_class)->apply_tag (buffer, tag, start, end);
	}
}

static void
add_source_mark (GtkSourceBuffer *buffer,
		 GtkSourceMark   *mark)
{
	const gchar *category;
	GtkSourceMarksSequence *seq;

	_gtk_source_marks_sequence_add (buffer->priv->all_source_marks,
					GTK_TEXT_MARK (mark));

	category = gtk_source_mark_get_category (mark);
	seq = g_hash_table_lookup (buffer->priv->source_marks, category);

	if (seq == NULL)
	{
		seq = _gtk_source_marks_sequence_new (GTK_TEXT_BUFFER (buffer));

		g_hash_table_insert (buffer->priv->source_marks,
				     g_strdup (category),
				     seq);
	}

	_gtk_source_marks_sequence_add (seq, GTK_TEXT_MARK (mark));
}

static void
gtk_source_buffer_real_mark_set	(GtkTextBuffer     *buffer,
				 const GtkTextIter *location,
				 GtkTextMark       *mark)
{
	if (GTK_SOURCE_IS_MARK (mark))
	{
		add_source_mark (GTK_SOURCE_BUFFER (buffer),
				 GTK_SOURCE_MARK (mark));

		g_signal_emit_by_name (buffer, "source_mark_updated", mark);
	}

	/* if the mark is the insert mark, update bracket matching */
	else if (mark == gtk_text_buffer_get_insert (buffer))
	{
		gtk_source_buffer_move_cursor (buffer, location, mark);
	}

	GTK_TEXT_BUFFER_CLASS (gtk_source_buffer_parent_class)->mark_set (buffer, location, mark);
}

static void
gtk_source_buffer_real_mark_deleted (GtkTextBuffer *buffer,
				     GtkTextMark   *mark)
{
	if (GTK_SOURCE_IS_MARK (mark))
	{
		GtkSourceBuffer *source_buffer = GTK_SOURCE_BUFFER (buffer);
		const gchar *category;
		GtkSourceMarksSequence *seq;

		category = gtk_source_mark_get_category (GTK_SOURCE_MARK (mark));
		seq = g_hash_table_lookup (source_buffer->priv->source_marks, category);

		if (_gtk_source_marks_sequence_is_empty (seq))
		{
			g_hash_table_remove (source_buffer->priv->source_marks, category);
		}

		g_signal_emit_by_name (buffer, "source_mark_updated", mark);
	}

	if (GTK_TEXT_BUFFER_CLASS (gtk_source_buffer_parent_class)->mark_deleted != NULL)
	{
		GTK_TEXT_BUFFER_CLASS (gtk_source_buffer_parent_class)->mark_deleted (buffer, mark);
	}
}

static void
gtk_source_buffer_real_undo (GtkSourceBuffer *buffer)
{
	g_return_if_fail (gtk_source_undo_manager_can_undo (buffer->priv->undo_manager));

	gtk_source_undo_manager_undo (buffer->priv->undo_manager);
}

static void
gtk_source_buffer_real_redo (GtkSourceBuffer *buffer)
{
	g_return_if_fail (gtk_source_undo_manager_can_redo (buffer->priv->undo_manager));

	gtk_source_undo_manager_redo (buffer->priv->undo_manager);
}

/**
 * gtk_source_buffer_create_source_mark:
 * @buffer: a #GtkSourceBuffer.
 * @name: (allow-none): the name of the mark, or %NULL.
 * @category: a string defining the mark category.
 * @where: location to place the mark.
 *
 * Creates a source mark in the @buffer of category @category.  A source mark is
 * a #GtkTextMark but organised into categories. Depending on the category
 * a pixbuf can be specified that will be displayed along the line of the mark.
 *
 * Like a #GtkTextMark, a #GtkSourceMark can be anonymous if the
 * passed @name is %NULL.  Also, the buffer owns the marks so you
 * shouldn't unreference it.
 *
 * Marks always have left gravity and are moved to the beginning of
 * the line when the user deletes the line they were in.
 *
 * Typical uses for a source mark are bookmarks, breakpoints, current
 * executing instruction indication in a source file, etc..
 *
 * Return value: (transfer none): a new #GtkSourceMark, owned by the buffer.
 *
 * Since: 2.2
 **/
GtkSourceMark *
gtk_source_buffer_create_source_mark (GtkSourceBuffer   *buffer,
				      const gchar       *name,
				      const gchar       *category,
				      const GtkTextIter *where)
{
	GtkSourceMark *mark;

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), NULL);
	g_return_val_if_fail (category != NULL, NULL);
	g_return_val_if_fail (where != NULL, NULL);

	mark = gtk_source_mark_new (name, category);
	gtk_text_buffer_add_mark (GTK_TEXT_BUFFER (buffer),
				  GTK_TEXT_MARK (mark),
				  where);

	return mark;
}

static GtkSourceMarksSequence *
get_marks_sequence (GtkSourceBuffer *buffer,
		    const gchar     *category)
{
	return category == NULL ?
		buffer->priv->all_source_marks :
		g_hash_table_lookup (buffer->priv->source_marks, category);
}

GtkSourceMark *
_gtk_source_buffer_source_mark_next (GtkSourceBuffer *buffer,
				     GtkSourceMark   *mark,
				     const gchar     *category)
{
	GtkSourceMarksSequence *seq;
	GtkTextMark *next_mark;

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), NULL);

	seq = get_marks_sequence (buffer, category);

	if (seq == NULL)
	{
		return NULL;
	}

	next_mark = _gtk_source_marks_sequence_next (seq, GTK_TEXT_MARK (mark));

	return next_mark == NULL ? NULL : GTK_SOURCE_MARK (next_mark);
}

GtkSourceMark *
_gtk_source_buffer_source_mark_prev (GtkSourceBuffer *buffer,
				     GtkSourceMark   *mark,
				     const gchar     *category)
{
	GtkSourceMarksSequence *seq;
	GtkTextMark *prev_mark;

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), NULL);

	seq = get_marks_sequence (buffer, category);

	if (seq == NULL)
	{
		return NULL;
	}

	prev_mark = _gtk_source_marks_sequence_prev (seq, GTK_TEXT_MARK (mark));

	return prev_mark == NULL ? NULL : GTK_SOURCE_MARK (prev_mark);
}

/**
 * gtk_source_buffer_forward_iter_to_source_mark:
 * @buffer: a #GtkSourceBuffer.
 * @iter: an iterator.
 * @category: (allow-none): category to search for, or %NULL
 *
 * Moves @iter to the position of the next #GtkSourceMark of the given
 * @category. Returns %TRUE if @iter was moved. If @category is NULL, the
 * next source mark can be of any category.
 *
 * Returns: whether @iter was moved.
 *
 * Since: 2.2
 **/
gboolean
gtk_source_buffer_forward_iter_to_source_mark (GtkSourceBuffer *buffer,
					       GtkTextIter     *iter,
					       const gchar     *category)
{
	GtkSourceMarksSequence *seq;

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);

	seq = get_marks_sequence (buffer, category);

	if (seq == NULL)
	{
		return FALSE;
	}

	return _gtk_source_marks_sequence_forward_iter (seq, iter);
}

/**
 * gtk_source_buffer_backward_iter_to_source_mark:
 * @buffer: a #GtkSourceBuffer.
 * @iter: an iterator.
 * @category: (allow-none): category to search for, or %NULL
 *
 * Moves @iter to the position of the previous #GtkSourceMark of the given
 * category. Returns %TRUE if @iter was moved. If @category is NULL, the
 * previous source mark can be of any category.
 *
 * Returns: whether @iter was moved.
 *
 * Since: 2.2
 **/
gboolean
gtk_source_buffer_backward_iter_to_source_mark (GtkSourceBuffer *buffer,
						GtkTextIter     *iter,
						const gchar     *category)
{
	GtkSourceMarksSequence *seq;

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);

	seq = get_marks_sequence (buffer, category);

	if (seq == NULL)
	{
		return FALSE;
	}

	return _gtk_source_marks_sequence_backward_iter (seq, iter);
}

/**
 * gtk_source_buffer_get_source_marks_at_iter:
 * @buffer: a #GtkSourceBuffer.
 * @iter: an iterator.
 * @category: (allow-none): category to search for, or %NULL
 *
 * Returns the list of marks of the given category at @iter. If @category
 * is %NULL it returns all marks at @iter.
 *
 * Returns: (element-type GtkSource.Mark) (transfer container):
 * a newly allocated #GSList.
 *
 * Since: 2.2
 **/
GSList *
gtk_source_buffer_get_source_marks_at_iter (GtkSourceBuffer *buffer,
					    GtkTextIter     *iter,
					    const gchar     *category)
{
	GtkSourceMarksSequence *seq;

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), NULL);
	g_return_val_if_fail (iter != NULL, NULL);

	seq = get_marks_sequence (buffer, category);

	if (seq == NULL)
	{
		return NULL;
	}

	return _gtk_source_marks_sequence_get_marks_at_iter (seq, iter);
}

/**
 * gtk_source_buffer_get_source_marks_at_line:
 * @buffer: a #GtkSourceBuffer.
 * @line: a line number.
 * @category: (allow-none): category to search for, or %NULL
 *
 * Returns the list of marks of the given category at @line.
 * If @category is %NULL, all marks at @line are returned.
 *
 * Returns: (element-type GtkSource.Mark) (transfer container):
 * a newly allocated #GSList.
 *
 * Since: 2.2
 **/
GSList *
gtk_source_buffer_get_source_marks_at_line (GtkSourceBuffer *buffer,
					    gint             line,
					    const gchar     *category)
{
	GtkSourceMarksSequence *seq;
	GtkTextIter start;
	GtkTextIter end;

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), NULL);

	seq = get_marks_sequence (buffer, category);

	if (seq == NULL)
	{
		return NULL;
	}

	gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (buffer),
					  &start,
					  line);

	end = start;

	if (!gtk_text_iter_ends_line (&end))
	{
		gtk_text_iter_forward_to_line_end (&end);
	}

	return _gtk_source_marks_sequence_get_marks_in_range (seq, &start, &end);
}

/**
 * gtk_source_buffer_remove_source_marks:
 * @buffer: a #GtkSourceBuffer.
 * @start: a #GtkTextIter.
 * @end: a #GtkTextIter.
 * @category: (allow-none): category to search for, or %NULL.
 *
 * Remove all marks of @category between @start and @end from the buffer.
 * If @category is NULL, all marks in the range will be removed.
 *
 * Since: 2.2
 **/
void
gtk_source_buffer_remove_source_marks (GtkSourceBuffer   *buffer,
				       const GtkTextIter *start,
				       const GtkTextIter *end,
				       const gchar       *category)
{
	GtkSourceMarksSequence *seq;
	GSList *list;
	GSList *l;

 	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));
 	g_return_if_fail (start != NULL);
 	g_return_if_fail (end != NULL);

	seq = get_marks_sequence (buffer, category);

	if (seq == NULL)
	{
		return;
	}

	list = _gtk_source_marks_sequence_get_marks_in_range (seq, start, end);

	for (l = list; l != NULL; l = l->next)
	{
		gtk_text_buffer_delete_mark (GTK_TEXT_BUFFER (buffer), l->data);
	}

	g_slist_free (list);
}

/**
 * gtk_source_buffer_iter_has_context_class:
 * @buffer: a #GtkSourceBuffer.
 * @iter: a #GtkTextIter.
 * @context_class: class to search for.
 *
 * Check if the class @context_class is set on @iter.
 *
 * See the #GtkSourceBuffer description for the list of default context classes.
 *
 * Returns: whether @iter has the context class.
 * Since: 2.10
 **/
gboolean
gtk_source_buffer_iter_has_context_class (GtkSourceBuffer   *buffer,
                                          const GtkTextIter *iter,
                                          const gchar       *context_class)
{
	GtkTextTag *tag;

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (context_class != NULL, FALSE);

	if (buffer->priv->highlight_engine == NULL)
	{
		return FALSE;
	}

	tag = _gtk_source_engine_get_context_class_tag (buffer->priv->highlight_engine,
							context_class);

	if (tag != NULL)
	{
		return gtk_text_iter_has_tag (iter, tag);
	}
	else
	{
		return FALSE;
	}
}

/**
 * gtk_source_buffer_get_context_classes_at_iter:
 * @buffer: a #GtkSourceBuffer.
 * @iter: a #GtkTextIter.
 *
 * Get all defined context classes at @iter.
 *
 * See the #GtkSourceBuffer description for the list of default context classes.
 *
 * Returns: (array zero-terminated=1) (transfer full): a new %NULL
 * terminated array of context class names.
 * Use g_strfreev() to free the array if it is no longer needed.
 *
 * Since: 2.10
 **/
gchar **
gtk_source_buffer_get_context_classes_at_iter (GtkSourceBuffer   *buffer,
                                               const GtkTextIter *iter)
{
	GSList *tags;
	GSList *item;
	GPtrArray *ret;

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), NULL);
	g_return_val_if_fail (iter != NULL, NULL);

	tags = gtk_text_iter_get_tags (iter);
	ret = g_ptr_array_new ();

	for (item = tags; item; item = g_slist_next (item))
	{
		gchar const *name = g_object_get_data (G_OBJECT (item->data),
		                                       TAG_CONTEXT_CLASS_NAME);

		if (name != NULL)
		{
			g_ptr_array_add (ret, g_strdup (name));
		}
	}

	g_slist_free (tags);

	g_ptr_array_add (ret, NULL);
	return (gchar **) g_ptr_array_free (ret, FALSE);
}

/**
 * gtk_source_buffer_iter_forward_to_context_class_toggle:
 * @buffer: a #GtkSourceBuffer.
 * @iter: (inout): a #GtkTextIter.
 * @context_class: the context class.
 *
 * Moves forward to the next toggle (on or off) of the context class. If no
 * matching context class toggles are found, returns %FALSE, otherwise %TRUE.
 * Does not return toggles located at @iter, only toggles after @iter. Sets
 * @iter to the location of the toggle, or to the end of the buffer if no
 * toggle is found.
 *
 * See the #GtkSourceBuffer description for the list of default context classes.
 *
 * Returns: whether we found a context class toggle after @iter
 *
 * Since: 2.10
 **/
gboolean
gtk_source_buffer_iter_forward_to_context_class_toggle (GtkSourceBuffer *buffer,
                                                        GtkTextIter     *iter,
                                                        const gchar     *context_class)
{
	GtkTextTag *tag;

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (context_class != NULL, FALSE);

	if (buffer->priv->highlight_engine == NULL)
	{
		return FALSE;
	}

	tag = _gtk_source_engine_get_context_class_tag (buffer->priv->highlight_engine,
							context_class);

	if (tag == NULL)
	{
		return FALSE;
	}
	else
	{
		return gtk_text_iter_forward_to_tag_toggle (iter, tag);
	}
}

/**
 * gtk_source_buffer_iter_backward_to_context_class_toggle:
 * @buffer: a #GtkSourceBuffer.
 * @iter: (inout): a #GtkTextIter.
 * @context_class: the context class.
 *
 * Moves backward to the next toggle (on or off) of the context class. If no
 * matching context class toggles are found, returns %FALSE, otherwise %TRUE.
 * Does not return toggles located at @iter, only toggles after @iter. Sets
 * @iter to the location of the toggle, or to the end of the buffer if no
 * toggle is found.
 *
 * See the #GtkSourceBuffer description for the list of default context classes.
 *
 * Returns: whether we found a context class toggle before @iter
 *
 * Since: 2.10
 **/
gboolean
gtk_source_buffer_iter_backward_to_context_class_toggle (GtkSourceBuffer *buffer,
                                                         GtkTextIter     *iter,
                                                         const gchar     *context_class)
{
	GtkTextTag *tag;

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (context_class != NULL, FALSE);

	if (buffer->priv->highlight_engine == NULL)
	{
		return FALSE;
	}

	tag = _gtk_source_engine_get_context_class_tag (buffer->priv->highlight_engine,
							context_class);

	if (tag == NULL)
	{
		return FALSE;
	}
	else
	{
		return gtk_text_iter_backward_to_tag_toggle (iter, tag);
	}
}

static gchar *
do_lower_case (GtkTextBuffer     *buffer,
	       const GtkTextIter *start,
	       const GtkTextIter *end)
{
	gchar *text;
	gchar *new_text;

	text = gtk_text_buffer_get_text (buffer, start, end, TRUE);
	new_text = g_utf8_strdown (text, -1);

	g_free (text);
	return new_text;
}

static gchar *
do_upper_case (GtkTextBuffer     *buffer,
	       const GtkTextIter *start,
	       const GtkTextIter *end)
{
	gchar *text;
	gchar *new_text;

	text = gtk_text_buffer_get_text (buffer, start, end, TRUE);
	new_text = g_utf8_strup (text, -1);

	g_free (text);
	return new_text;
}

static gchar *
do_toggle_case (GtkTextBuffer     *buffer,
		const GtkTextIter *start,
		const GtkTextIter *end)
{
	GString *str;
	GtkTextIter iter_start;

	str = g_string_new (NULL);
	iter_start = *start;

	while (!gtk_text_iter_is_end (&iter_start))
	{
		GtkTextIter iter_end;
		gchar *text;
		gchar *text_down;
		gchar *text_up;

		iter_end = iter_start;
		gtk_text_iter_forward_cursor_position (&iter_end);

		if (gtk_text_iter_compare (end, &iter_end) < 0)
		{
			break;
		}

		text = gtk_text_buffer_get_text (buffer, &iter_start, &iter_end, TRUE);
		text_down = g_utf8_strdown (text, -1);
		text_up = g_utf8_strup (text, -1);

		if (g_strcmp0 (text, text_down) == 0)
		{
			g_string_append (str, text_up);
		}
		else if (g_strcmp0 (text, text_up) == 0)
		{
			g_string_append (str, text_down);
		}
		else
		{
			g_string_append (str, text);
		}

		g_free (text);
		g_free (text_down);
		g_free (text_up);

		iter_start = iter_end;
	}

	return g_string_free (str, FALSE);
}

static gchar *
do_title_case (GtkTextBuffer     *buffer,
	       const GtkTextIter *start,
	       const GtkTextIter *end)
{
	GString *str;
	GtkTextIter iter_start;

	str = g_string_new (NULL);
	iter_start = *start;

	while (!gtk_text_iter_is_end (&iter_start))
	{
		GtkTextIter iter_end;
		gchar *text;

		iter_end = iter_start;
		gtk_text_iter_forward_cursor_position (&iter_end);

		if (gtk_text_iter_compare (end, &iter_end) < 0)
		{
			break;
		}

		text = gtk_text_buffer_get_text (buffer, &iter_start, &iter_end, TRUE);

		if (gtk_text_iter_starts_word (&iter_start))
		{
			gchar *text_normalized;

			text_normalized = g_utf8_normalize (text, -1, G_NORMALIZE_DEFAULT);

			if (g_utf8_strlen (text_normalized, -1) == 1)
			{
				gunichar c;
				gunichar new_c;

				c = gtk_text_iter_get_char (&iter_start);
				new_c = g_unichar_totitle (c);

				g_string_append_unichar (str, new_c);
			}
			else
			{
				gchar *text_up;

				text_up = g_utf8_strup (text, -1);
				g_string_append (str, text_up);

				g_free (text_up);
			}

			g_free (text_normalized);
		}
		else
		{
			gchar *text_down;

			text_down = g_utf8_strdown (text, -1);
			g_string_append (str, text_down);

			g_free (text_down);
		}

		g_free (text);
		iter_start = iter_end;
	}

	return g_string_free (str, FALSE);
}

/**
 * gtk_source_buffer_change_case:
 * @buffer: a #GtkSourceBuffer.
 * @case_type: how to change the case.
 * @start: a #GtkTextIter.
 * @end: a #GtkTextIter.
 *
 * Changes the case of the text between the specified iterators.
 *
 * Since: 3.12
 **/
void
gtk_source_buffer_change_case (GtkSourceBuffer         *buffer,
                               GtkSourceChangeCaseType  case_type,
                               GtkTextIter             *start,
                               GtkTextIter             *end)
{
	GtkTextBuffer *text_buffer;
	gchar *new_text;

	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));
	g_return_if_fail (start != NULL);
	g_return_if_fail (end != NULL);

	gtk_text_iter_order (start, end);

	text_buffer = GTK_TEXT_BUFFER (buffer);

	switch (case_type)
	{
		case GTK_SOURCE_CHANGE_CASE_LOWER:
			new_text = do_lower_case (text_buffer, start, end);
			break;

		case GTK_SOURCE_CHANGE_CASE_UPPER:
			new_text = do_upper_case (text_buffer, start, end);
			break;

		case GTK_SOURCE_CHANGE_CASE_TOGGLE:
			new_text = do_toggle_case (text_buffer, start, end);
			break;

		case GTK_SOURCE_CHANGE_CASE_TITLE:
			new_text = do_title_case (text_buffer, start, end);
			break;

		default:
			g_return_if_reached ();
	}

	gtk_text_buffer_begin_user_action (text_buffer);
	gtk_text_buffer_delete (text_buffer, start, end);
	gtk_text_buffer_insert (text_buffer, start, new_text, -1);
	gtk_text_buffer_end_user_action (text_buffer);

	g_free (new_text);
}

/**
 * gtk_source_buffer_set_undo_manager:
 * @buffer: a #GtkSourceBuffer.
 * @manager: (allow-none): A #GtkSourceUndoManager or %NULL.
 *
 * Set the buffer undo manager. If @manager is %NULL the default undo manager
 * will be set.
 **/
void
gtk_source_buffer_set_undo_manager (GtkSourceBuffer      *buffer,
                                    GtkSourceUndoManager *manager)
{
	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));
	g_return_if_fail (manager == NULL || GTK_SOURCE_IS_UNDO_MANAGER (manager));

	if (manager == NULL)
	{
		manager = g_object_new (GTK_SOURCE_TYPE_UNDO_MANAGER_DEFAULT,
		                        "buffer", buffer,
		                        "max-undo-levels", buffer->priv->max_undo_levels,
		                        NULL);
	}
	else
	{
		g_object_ref (manager);
	}

	set_undo_manager (buffer, manager);
	g_object_unref (manager);

	g_object_notify (G_OBJECT (buffer), "undo-manager");
}

/**
 * gtk_source_buffer_get_undo_manager:
 * @buffer: a #GtkSourceBuffer.
 *
 * Returns the #GtkSourceUndoManager associated with the buffer,
 * see gtk_source_buffer_set_undo_manager().  The returned object should not be
 * unreferenced by the user.
 *
 * Returns: (nullable) (transfer none): the #GtkSourceUndoManager associated
 * with the buffer, or %NULL.
 **/
GtkSourceUndoManager *
gtk_source_buffer_get_undo_manager (GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), NULL);

	return buffer->priv->undo_manager;
}

void
_gtk_source_buffer_add_search_context (GtkSourceBuffer        *buffer,
				       GtkSourceSearchContext *search_context)
{
	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));
	g_return_if_fail (GTK_SOURCE_IS_SEARCH_CONTEXT (search_context));
	g_return_if_fail (gtk_source_search_context_get_buffer (search_context) == buffer);

	if (g_list_find (buffer->priv->search_contexts, search_context) != NULL)
	{
		return;
	}

	buffer->priv->search_contexts = g_list_prepend (buffer->priv->search_contexts,
							search_context);

	g_object_weak_ref (G_OBJECT (search_context),
			   (GWeakNotify)search_context_weak_notify_cb,
			   buffer);
}

static void
sync_invalid_char_tag (GtkSourceBuffer *buffer,
		       GParamSpec      *pspec,
		       gpointer         data)
{
	GtkSourceStyle *style = NULL;

	if (buffer->priv->style_scheme != NULL)
	{
		style = gtk_source_style_scheme_get_style (buffer->priv->style_scheme, "def:error");
	}

	_gtk_source_style_apply (style, buffer->priv->invalid_char_tag);
}

static void
text_tag_set_highest_priority (GtkTextTag    *tag,
			       GtkTextBuffer *buffer)
{
	GtkTextTagTable *table;
	gint n;

	table = gtk_text_buffer_get_tag_table (buffer);
	n = gtk_text_tag_table_get_size (table);
	gtk_text_tag_set_priority (tag, n - 1);
}

void
_gtk_source_buffer_set_as_invalid_character (GtkSourceBuffer   *buffer,
					     const GtkTextIter *start,
					     const GtkTextIter *end)
{
	if (buffer->priv->invalid_char_tag == NULL)
	{
		buffer->priv->invalid_char_tag = gtk_text_buffer_create_tag (GTK_TEXT_BUFFER (buffer),
									     "invalid-char-style",
									     NULL);

		sync_invalid_char_tag (buffer, NULL, NULL);

		g_signal_connect (buffer,
		                  "notify::style-scheme",
		                  G_CALLBACK (sync_invalid_char_tag),
		                  NULL);
	}

	/* Make sure the 'error' tag has the priority over
	 * syntax highlighting tags.
	 */
	text_tag_set_highest_priority (buffer->priv->invalid_char_tag,
	                               GTK_TEXT_BUFFER (buffer));

	gtk_text_buffer_apply_tag (GTK_TEXT_BUFFER (buffer),
	                           buffer->priv->invalid_char_tag,
	                           start,
	                           end);
}

gboolean
_gtk_source_buffer_has_invalid_chars (GtkSourceBuffer *buffer)
{
	GtkTextIter start;

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), FALSE);

	if (buffer->priv->invalid_char_tag == NULL)
	{
		return FALSE;
	}

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &start);

	if (gtk_text_iter_begins_tag (&start, buffer->priv->invalid_char_tag) ||
	    gtk_text_iter_forward_to_tag_toggle (&start, buffer->priv->invalid_char_tag))
	{
		return TRUE;
	}

	return FALSE;
}

/**
 * gtk_source_buffer_set_implicit_trailing_newline:
 * @buffer: a #GtkSourceBuffer.
 * @implicit_trailing_newline: the new value.
 *
 * Sets whether the @buffer has an implicit trailing newline.
 *
 * If an explicit trailing newline is present in a #GtkTextBuffer, #GtkTextView
 * shows it as an empty line. This is generally not what the user expects.
 *
 * If @implicit_trailing_newline is %TRUE (the default value):
 *  - when a #GtkSourceFileLoader loads the content of a file into the @buffer,
 *    the trailing newline (if present in the file) is not inserted into the
 *    @buffer.
 *  - when a #GtkSourceFileSaver saves the content of the @buffer into a file, a
 *    trailing newline is added to the file.
 *
 * On the other hand, if @implicit_trailing_newline is %FALSE, the file's
 * content is not modified when loaded into the @buffer, and the @buffer's
 * content is not modified when saved into a file.
 *
 * Since: 3.14
 */
void
gtk_source_buffer_set_implicit_trailing_newline (GtkSourceBuffer *buffer,
						 gboolean         implicit_trailing_newline)
{
	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));

	implicit_trailing_newline = implicit_trailing_newline != FALSE;

	if (buffer->priv->implicit_trailing_newline != implicit_trailing_newline)
	{
		buffer->priv->implicit_trailing_newline = implicit_trailing_newline;
		g_object_notify (G_OBJECT (buffer), "implicit-trailing-newline");
	}
}

/**
 * gtk_source_buffer_get_implicit_trailing_newline:
 * @buffer: a #GtkSourceBuffer.
 *
 * Returns: whether the @buffer has an implicit trailing newline.
 * Since: 3.14
 */
gboolean
gtk_source_buffer_get_implicit_trailing_newline (GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), TRUE);

	return buffer->priv->implicit_trailing_newline;
}
