/* hev-plugin.c
 * Heiher <admin@heiher.info>
 */

#define XP_UNIX		1

#include <stdio.h>
#include <string.h>
#include <nspr.h>
#include <npapi.h>
#include <npruntime.h>
#include <npfunctions.h>
#include <gtk/gtk.h>

#include "hev-plugin.h"

#define PLUGIN_NAME         "HevPlugin"
#define PLUGIN_MIME_TYPES   "application/x-hevplugin"
#define PLUGIN_DESCRIPTION  "HevPlugin demo"

typedef struct _HevPluginPrivate HevPluginPrivate;

struct _HevPluginPrivate
{
	NPWindow *window;
	GtkWidget *entry;
	NPObject *obj;
};

typedef struct _HevScriptableObj HevScriptableObj;

struct _HevScriptableObj
{
	NPObject obj;
	NPP npp;
};

static NPNetscapeFuncs *netscape_funcs;
static NPClass npklass = { 0 };
static NPIdentifier npi_get_text = { 0 };
static NPIdentifier npi_set_text = { 0 };

static NPObject * NPO_Allocate(NPP npp, NPClass *klass);
static void NPO_Deallocate(NPObject *npobj);
static bool NPO_HasMethod(NPObject *npobj, NPIdentifier method_name);
static bool NPO_Invoke(NPObject *npobj, NPIdentifier name,
			const NPVariant *args, uint32_t arg_count,
			NPVariant *result);

static void g_debug_log_null_handler(const gchar *domain,
			GLogLevelFlags level, const gchar *message,
			gpointer user_data);

NP_EXPORT(NPError) NP_Initialize(NPNetscapeFuncs *npn_funcs, NPPluginFuncs *npp_funcs)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	/* Check errors */
	if((NULL==npn_funcs) || (NULL==npp_funcs))
	  return NPERR_INVALID_FUNCTABLE_ERROR;

	/* Check version and struct size */
	if(NP_VERSION_MAJOR < (npn_funcs->version>>8))
	  return NPERR_INCOMPATIBLE_VERSION_ERROR;
	if(sizeof(NPNetscapeFuncs) > npn_funcs->size)
	  return NPERR_INVALID_FUNCTABLE_ERROR;
	if(sizeof(NPPluginFuncs) > npp_funcs->size)
	  return NPERR_INVALID_FUNCTABLE_ERROR;

	/* Save netscape functions */
	netscape_funcs = npn_funcs;

	/* Overwrite plugin functions */
	npp_funcs->version = (NP_VERSION_MAJOR<<8) + NP_VERSION_MINOR;
	npp_funcs->size = sizeof(NPPluginFuncs);
	npp_funcs->newp = NPP_New;
	npp_funcs->destroy = NPP_Destroy;
	npp_funcs->setwindow = NPP_SetWindow;
	npp_funcs->newstream = NPP_NewStream;
	npp_funcs->destroystream = NPP_DestroyStream;
	npp_funcs->writeready = NPP_WriteReady;
	npp_funcs->write= NPP_Write;
	npp_funcs->asfile = NPP_StreamAsFile;
	npp_funcs->print = NPP_Print;
	npp_funcs->urlnotify = NPP_URLNotify;
	npp_funcs->event = NPP_HandleEvent;
	npp_funcs->getvalue = NPP_GetValue;

	/* NP Object Class */
	npklass.structVersion = NP_CLASS_STRUCT_VERSION;
	npklass.allocate = NPO_Allocate;
	npklass.deallocate = NPO_Deallocate;
	npklass.hasMethod = NPO_HasMethod;
	npklass.invoke = NPO_Invoke;

	/* NP Object method property identifier */
	npi_get_text = NPN_GetStringIdentifier("get_text");
	npi_set_text = NPN_GetStringIdentifier("set_text");

	/* GTK+ Initialize */
	gtk_init(0, 0);

	return NPERR_NO_ERROR;
}

NP_EXPORT(NPError) NP_Shutdown(void)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	return NPERR_NO_ERROR;
}

NP_EXPORT(const char *) NP_GetMIMEDescription(void)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	return PLUGIN_MIME_TYPES":"PLUGIN_NAME":"PLUGIN_DESCRIPTION;
}

NP_EXPORT(NPError) NP_GetValue(void *future, NPPVariable variable, void *value)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	return NPP_GetValue(future, variable, value);
}

NPError NPN_SetValue(NPP instance, NPPVariable variable, void *value)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	return netscape_funcs->setvalue(instance, variable, value);
}

NPError NPN_GetValue(NPP instance, NPNVariable variable, void *value)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	return netscape_funcs->getvalue(instance, variable, value);
}

NPError NPN_DestroyStream(NPP instance, NPStream *stream,
			NPReason reason)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	return netscape_funcs->destroystream(instance, stream, reason);
}

void * NPN_MemAlloc(uint32_t size)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	return netscape_funcs->memalloc(size);
}

void NPN_MemFree(void *ptr)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	netscape_funcs->memfree(ptr);
}

NPIdentifier NPN_GetStringIdentifier(const NPUTF8 *name)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	return netscape_funcs->getstringidentifier(name);
}

NPUTF8 *NPN_UTF8FromIdentifier(NPIdentifier identifier)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	return netscape_funcs->utf8fromidentifier(identifier);
}

NPObject *NPN_CreateObject(NPP instance, NPClass *klass)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	return netscape_funcs->createobject(instance, klass);
}

NPObject *NPN_RetainObject(NPObject *npobj)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	return netscape_funcs->retainobject(npobj);
}

void NPN_ReleaseObject(NPObject *npobj)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	return netscape_funcs->releaseobject(npobj);
}

NPError NPP_New(NPMIMEType plugin_type, NPP instance,
			uint16_t mode, int16_t argc, char* argn[], char* argv[],
			NPSavedData *saved)
{
	HevPluginPrivate *priv = NULL;
	NPError err = NPERR_NO_ERROR;
	PRBool xembed = PR_FALSE;
	NPNToolkitType toolkit = 0;
	uint16_t i = 0;
	gboolean debug = FALSE;

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

    err = NPN_GetValue(instance, NPNVSupportsXEmbedBool, &xembed);
    if((NPERR_NO_ERROR!=err) || (PR_TRUE!=xembed))
    {
		g_debug("%s:%d[%s]=>(%s)", __FILE__, __LINE__,
					__FUNCTION__, "XEmbed nonsupport!");
        return NPERR_GENERIC_ERROR;
    }

    err = NPN_GetValue(instance, NPNVToolkit, &toolkit);
    if((NPERR_NO_ERROR!=err) || (NPNVGtk2!=toolkit))
    {
		g_debug("%s:%d[%s]=>(%s)", __FILE__, __LINE__,
					__FUNCTION__, "GTK+ Toolkit isn't 2!");
        return NPERR_GENERIC_ERROR;
    }

	priv = NPN_MemAlloc(sizeof(HevPluginPrivate));
	if(NULL == priv)
	{
		g_debug("%s:%d[%s]=>(%s)", __FILE__, __LINE__,
					__FUNCTION__, "Alloc private data failed!");
		return NPERR_OUT_OF_MEMORY_ERROR;
	}
	memset(priv, 0, sizeof(HevPluginPrivate));
	instance->pdata = priv;

	/* Params */
	for(i=0; i<argc; i++)
	{
		if((0==g_strcmp0(argn[i], "debug")) &&
					(0==g_ascii_strcasecmp(argv[i], "true")))
		  debug = TRUE;
	}

	/* Reset debug log handler */
	if(FALSE == debug)
	  g_log_set_handler(NULL, G_LOG_LEVEL_DEBUG,
				  g_debug_log_null_handler, NULL);

	return NPERR_NO_ERROR;
}

NPError NPP_Destroy(NPP instance, NPSavedData **saved)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	NPN_MemFree(instance->pdata);

	return NPERR_NO_ERROR;
}

NPError NPP_SetWindow(NPP instance, NPWindow *window)
{
	HevPluginPrivate *priv = (HevPluginPrivate*)instance->pdata;
	GtkWidget *plug = NULL;

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);
	
	g_return_val_if_fail(instance, NPERR_INVALID_INSTANCE_ERROR);

	/* Check re-enter */
	if(priv->window == window)
	  return NPERR_NO_ERROR;

	priv->window = window;

	plug = gtk_plug_new((GdkNativeWindow)window->window);
	gtk_widget_show(plug);

	priv->entry = gtk_entry_new();
	gtk_container_add(GTK_CONTAINER(plug), priv->entry);
	gtk_widget_show(priv->entry);

	return NPERR_NO_ERROR;
}

NPError NPP_NewStream(NPP instance, NPMIMEType type,
			NPStream *stream, NPBool seekable, uint16_t *stype)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	return NPERR_NO_ERROR;
}

NPError NPP_DestroyStream(NPP instance, NPStream *stream,
			NPReason reason)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	return NPERR_NO_ERROR;
}

int32_t NPP_WriteReady(NPP instance, NPStream *stream)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	g_return_val_if_fail(instance, NPERR_INVALID_INSTANCE_ERROR);

	NPN_DestroyStream(instance, stream, NPRES_DONE);

	return -1L;
}

int32_t NPP_Write(NPP instance, NPStream *stream,
			int32_t offset, int32_t len, void *buffer)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	g_return_val_if_fail(instance, NPERR_INVALID_INSTANCE_ERROR);

	NPN_DestroyStream(instance, stream, NPRES_DONE);

	return -1L;
}

void NPP_StreamAsFile(NPP instance, NPStream *stream,
			const char *fname)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);
}

void NPP_Print(NPP instance, NPPrint *platform_print)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);
}

int16_t NPP_HandleEvent(NPP instance, void *event)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	return 0;
}

void NPP_URLNotify(NPP instance, const char *url, NPReason reason,
			void *notify_data)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);
}

NPError NPP_GetValue(NPP instance, NPPVariable variable,
			void *value)
{
	HevPluginPrivate *priv = NULL;

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	if(instance)
	  priv = (HevPluginPrivate *)instance->pdata;

	switch(variable)
	{
	case NPPVpluginNameString:
		*((char **)value) = PLUGIN_NAME;
		break;
	case NPPVpluginDescriptionString:
		*((char **)value) = PLUGIN_DESCRIPTION;
		break;
	case NPPVpluginNeedsXEmbed:
		*((PRBool *)value) = PR_TRUE;
		break;
	case NPPVpluginScriptableNPObject:
		if(NULL == priv->obj)
			priv->obj = NPN_CreateObject(instance, &npklass);
		else
		  priv->obj = NPN_RetainObject(priv->obj);
		*((NPObject **)value)  = priv->obj;
		break;
	default:
		return NPERR_GENERIC_ERROR;
	}

	return NPERR_NO_ERROR;
}

static NPObject * NPO_Allocate(NPP npp, NPClass *klass)
{
	HevScriptableObj *obj = NULL;

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	obj = g_slice_new0(HevScriptableObj);
	obj->obj._class = klass;
	obj->npp = npp;

	return (NPObject *)obj;
}

static void NPO_Deallocate(NPObject *npobj)
{
	HevScriptableObj *obj = (HevScriptableObj *)npobj;

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	g_slice_free(HevScriptableObj, obj);
}

static bool NPO_Invoke(NPObject *npobj, NPIdentifier name,
			const NPVariant *args, uint32_t arg_count,
			NPVariant *result)
{
	HevScriptableObj *obj = (HevScriptableObj *)npobj;
	HevPluginPrivate *priv = (HevPluginPrivate *)obj->npp->pdata;

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	if(name == npi_get_text)
	{
		const gchar *text = NULL;

		text = gtk_entry_get_text(GTK_ENTRY(priv->entry));
		STRINGZ_TO_NPVARIANT(g_strdup(text), *result);

		return true;
	}
	else if(name == npi_set_text)
	{
		if(1 == arg_count)
		{
			GtkEntryBuffer *entry_buffer = NULL;
			NPString str = NPVARIANT_TO_STRING(args[0]);

			entry_buffer = gtk_entry_get_buffer(GTK_ENTRY(priv->entry));
			gtk_entry_buffer_set_text(entry_buffer, str.UTF8Characters,
						str.UTF8Length);

			return true;
		}
	}

	return false;
}

static bool NPO_HasMethod(NPObject *npobj, NPIdentifier method_name)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	if(method_name == npi_get_text)
	  return true;
	else if(method_name == npi_set_text)
	  return true;

	return false;
}

static void g_debug_log_null_handler(const gchar *domain,
			GLogLevelFlags level, const gchar *message,
			gpointer user_data)
{
}

