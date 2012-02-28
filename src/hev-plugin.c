/* hev-plugin.c
 * Heiher <admin@heiher.info>
 */

#define XP_UNIX		1

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <nspr.h>
#include <npapi.h>
#include <npruntime.h>
#include <npfunctions.h>
#include <gio/gio.h>

#include "hev-plugin.h"

#define PLUGIN_NAME         "HevConsoleKit"
#define PLUGIN_MIME_TYPES   "application/x-hevconsolekit"
#define PLUGIN_DESCRIPTION  "ConsoleKit controller plugin."

typedef struct _HevPluginPrivate HevPluginPrivate;

struct _HevPluginPrivate
{
	NPObject *obj;
	GDBusProxy *dbus_proxy;
};

typedef struct _HevScriptableObj HevScriptableObj;

struct _HevScriptableObj
{
	NPObject obj;
	NPP npp;
};

static NPNetscapeFuncs *netscape_funcs = NULL;
static NPClass npklass = { 0 };
static NPIdentifier npi_can_restart = { 0 };
static NPIdentifier npi_restart = { 0 };
static NPIdentifier npi_can_stop = { 0 };
static NPIdentifier npi_stop = { 0 };

static NPObject * NPO_Allocate(NPP npp, NPClass *klass);
static void NPO_Deallocate(NPObject *npobj);
static bool NPO_HasMethod(NPObject *npobj, NPIdentifier method_name);
static bool NPO_Invoke(NPObject *npobj, NPIdentifier name,
			const NPVariant *args, uint32_t arg_count,
			NPVariant *result);

static void g_debug_log_null_handler(const gchar *domain,
			GLogLevelFlags level, const gchar *message,
			gpointer user_data);

static void g_dbus_proxy_new_for_bus_handler(GObject *source_object,
			GAsyncResult *res, gpointer user_data);
static void g_dbus_proxy_call_restart_handler(GObject *source_object,
			GAsyncResult *res, gpointer user_data);
static void g_dbus_proxy_call_can_restart_handler(GObject *source_object,
			GAsyncResult *res, gpointer user_data);
static void g_dbus_proxy_call_stop_handler(GObject *source_object,
			GAsyncResult *res, gpointer user_data);
static void g_dbus_proxy_call_can_stop_handler(GObject *source_object,
			GAsyncResult *res, gpointer user_data);

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
	npi_can_restart = NPN_GetStringIdentifier("CanRestart");
	npi_restart = NPN_GetStringIdentifier("Restart");
	npi_can_stop = NPN_GetStringIdentifier("CanStop");
	npi_stop = NPN_GetStringIdentifier("Stop");

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

bool NPN_InvokeDefault(NPP npp, NPObject *npobj, const NPVariant *args,
                       uint32_t arg_count, NPVariant *result)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	return netscape_funcs->invokeDefault(npp, npobj,
				args, arg_count, result);
}

bool NPN_GetProperty(NPP npp, NPObject *npobj, NPIdentifier name,
			NPVariant *result)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	return netscape_funcs->getproperty(npp, npobj, name, result);
}

void NPN_ReleaseVariantValue(NPVariant *variant)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	netscape_funcs->releasevariantvalue(variant);
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

	/* DBus Proxy */
	g_dbus_proxy_new_for_bus(G_BUS_TYPE_SYSTEM,
				G_DBUS_PROXY_FLAGS_NONE, NULL,
				"org.freedesktop.ConsoleKit",
				"/org/freedesktop/ConsoleKit/Manager",
				"org.freedesktop.ConsoleKit.Manager",
				NULL,
				g_dbus_proxy_new_for_bus_handler,
				instance);

	return NPERR_NO_ERROR;
}

NPError NPP_Destroy(NPP instance, NPSavedData **saved)
{
	HevPluginPrivate *priv = (HevPluginPrivate *)instance->pdata;

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	g_object_unref(G_OBJECT(priv->dbus_proxy));
	NPN_MemFree(instance->pdata);

	return NPERR_NO_ERROR;
}

NPError NPP_SetWindow(NPP instance, NPWindow *window)
{
	HevPluginPrivate *priv = (HevPluginPrivate*)instance->pdata;

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);
	
	g_return_val_if_fail(instance, NPERR_INVALID_INSTANCE_ERROR);

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
	NPObject *func_obj = NULL;
	NPObject *window_obj = NULL, *location_obj = NULL;
	NPIdentifier identifier = 0;
	NPVariant var = { 0 };
	NPString protocol = { 0 };

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);
	g_return_val_if_fail(priv->dbus_proxy, PR_TRUE);
	g_return_val_if_fail((1==arg_count), PR_TRUE);
	g_return_val_if_fail(NPVARIANT_IS_OBJECT(args[0]), PR_TRUE);

	func_obj = NPVARIANT_TO_OBJECT(args[0]);

	/* Security check */
	NPN_GetValue(obj->npp, NPNVWindowNPObject, &window_obj);
	identifier = NPN_GetStringIdentifier("location");
	if(!NPN_GetProperty(obj->npp, window_obj, identifier, &var))
	{
		NPN_ReleaseObject(window_obj);
		return PR_FALSE;
	}

	location_obj = NPVARIANT_TO_OBJECT(var);
	NPN_ReleaseObject(window_obj);

	identifier = NPN_GetStringIdentifier("protocol");
	if(!NPN_GetProperty(obj->npp, location_obj, identifier, &var))
	{
		NPN_ReleaseObject(location_obj);
		return PR_FALSE;
	}

	protocol = NPVARIANT_TO_STRING(var);
	if(0 != g_ascii_strncasecmp(protocol.UTF8Characters,
					"file:", 5))
	{
		NPN_ReleaseVariantValue(&var);
		NPN_ReleaseObject(location_obj);
		return PR_FALSE;
	}

	NPN_ReleaseVariantValue(&var);
	NPN_ReleaseObject(location_obj);

	if(name == npi_restart)
	{
		NPN_RetainObject(func_obj);
		g_dbus_proxy_call(priv->dbus_proxy,
					"Restart", NULL,
					G_DBUS_CALL_FLAGS_NONE,
					5000, NULL,
					g_dbus_proxy_call_restart_handler,
					(gpointer)func_obj);

		return PR_TRUE;
	}
	else if(name == npi_can_restart)
	{
		NPN_RetainObject(func_obj);
		g_dbus_proxy_call(priv->dbus_proxy,
					"CanRestart", NULL,
					G_DBUS_CALL_FLAGS_NONE,
					5000, NULL,
					g_dbus_proxy_call_can_restart_handler,
					(gpointer)func_obj);

		return PR_TRUE;
	}
	else if(name == npi_stop)
	{
		NPN_RetainObject(func_obj);
		g_dbus_proxy_call(priv->dbus_proxy,
					"Stop", NULL,
					G_DBUS_CALL_FLAGS_NONE,
					5000, NULL,
					g_dbus_proxy_call_stop_handler,
					(gpointer)func_obj);

		return PR_TRUE;
	}
	else if(name == npi_can_stop)
	{
		NPN_RetainObject(func_obj);
		g_dbus_proxy_call(priv->dbus_proxy,
					"CanStop", NULL,
					G_DBUS_CALL_FLAGS_NONE,
					5000, NULL,
					g_dbus_proxy_call_can_stop_handler,
					(gpointer)func_obj);

		return PR_TRUE;
	}

	return PR_TRUE;
}

static bool NPO_HasMethod(NPObject *npobj, NPIdentifier method_name)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	if(method_name == npi_can_restart)
	  return PR_TRUE;
	else if(method_name == npi_restart)
	  return PR_TRUE;
	else if(method_name == npi_can_stop)
	  return PR_TRUE;
	else if(method_name == npi_stop)
	  return PR_TRUE;

	return PR_TRUE;
}

static void g_debug_log_null_handler(const gchar *domain,
			GLogLevelFlags level, const gchar *message,
			gpointer user_data)
{
}

static void g_dbus_proxy_new_for_bus_handler(GObject *source_object,
			GAsyncResult *res, gpointer user_data)
{
	NPP instance = user_data;
	HevPluginPrivate *priv = (HevPluginPrivate *)instance->pdata;

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	priv->dbus_proxy = g_dbus_proxy_new_for_bus_finish(res, NULL);
	g_object_set_data(G_OBJECT(priv->dbus_proxy), "npp", instance);
}

static void g_dbus_proxy_call_restart_handler(GObject *source_object,
			GAsyncResult *res, gpointer user_data)
{
	NPP instance = NULL;
	NPObject *func_obj = user_data;
	GVariant *retval = NULL;
	NPVariant val = { 0 }, rval = { 0 };

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	instance = g_object_get_data(source_object, "npp");

	retval = g_dbus_proxy_call_finish(G_DBUS_PROXY(source_object),
				res, NULL);

	if(retval)
	{
		BOOLEAN_TO_NPVARIANT(PR_TRUE, val);
		g_variant_unref(retval);
	}
	else
	{
		NULL_TO_NPVARIANT(val);
	}

	NPN_InvokeDefault(instance, func_obj, &val, 1, &rval);

	NPN_ReleaseVariantValue(&val);
	NPN_ReleaseVariantValue(&rval);
	NPN_ReleaseObject(func_obj);
}

static void g_dbus_proxy_call_can_restart_handler(GObject *source_object,
			GAsyncResult *res, gpointer user_data)
{
	NPP instance = NULL;
	NPObject *func_obj = user_data;
	GVariant *retval = NULL;
	NPVariant val = { 0 }, rval = { 0 };

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	instance = g_object_get_data(source_object, "npp");

	retval = g_dbus_proxy_call_finish(G_DBUS_PROXY(source_object),
				res, NULL);

	if(retval)
	{
		GVariant *vbool = NULL;

		vbool = g_variant_get_child_value(retval, 0);
		BOOLEAN_TO_NPVARIANT(g_variant_get_boolean(vbool), val);

		g_variant_unref(retval);
	}
	else
	{
		NULL_TO_NPVARIANT(val);
	}

	NPN_InvokeDefault(instance, func_obj, &val, 1, &rval);

	NPN_ReleaseVariantValue(&val);
	NPN_ReleaseVariantValue(&rval);
	NPN_ReleaseObject(func_obj);
}

static void g_dbus_proxy_call_stop_handler(GObject *source_object,
			GAsyncResult *res, gpointer user_data)
{
	NPP instance = NULL;
	NPObject *func_obj = user_data;
	GVariant *retval = NULL;
	NPVariant val = { 0 }, rval = { 0 };

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	instance = g_object_get_data(source_object, "npp");

	retval = g_dbus_proxy_call_finish(G_DBUS_PROXY(source_object),
				res, NULL);

	if(retval)
	{
		BOOLEAN_TO_NPVARIANT(PR_TRUE, val);
		g_variant_unref(retval);
	}
	else
	{
		NULL_TO_NPVARIANT(val);
	}

	NPN_InvokeDefault(instance, func_obj, &val, 1, &rval);

	NPN_ReleaseVariantValue(&val);
	NPN_ReleaseVariantValue(&rval);
	NPN_ReleaseObject(func_obj);
}

static void g_dbus_proxy_call_can_stop_handler(GObject *source_object,
			GAsyncResult *res, gpointer user_data)
{
	NPP instance = NULL;
	NPObject *func_obj = user_data;
	GVariant *retval = NULL;
	NPVariant val = { 0 }, rval = { 0 };

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	instance = g_object_get_data(source_object, "npp");

	retval = g_dbus_proxy_call_finish(G_DBUS_PROXY(source_object),
				res, NULL);

	if(retval)
	{
		GVariant *vbool = NULL;

		vbool = g_variant_get_child_value(retval, 0);
		BOOLEAN_TO_NPVARIANT(g_variant_get_boolean(vbool), val);

		g_variant_unref(retval);
	}
	else
	{
		NULL_TO_NPVARIANT(val);
	}

	NPN_InvokeDefault(instance, func_obj, &val, 1, &rval);

	NPN_ReleaseVariantValue(&val);
	NPN_ReleaseVariantValue(&rval);
	NPN_ReleaseObject(func_obj);
}

