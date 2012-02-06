/* hev-plugin.h
 * Heiher <admin@heiher.info>
 */

#ifndef __HEV_PLUGIN_H__
#define __HEV_PLUGIN_H__

NP_EXPORT(NPError) NP_Initialize(NPNetscapeFuncs *npn_funcs, NPPluginFuncs *npp_funcs);
NP_EXPORT(NPError) NP_Shutdown(void);
NP_EXPORT(const char *) NP_GetMIMEDescription(void);
NP_EXPORT(NPError) NP_GetValue(void *future, NPPVariable variable, void *value);

#endif /* __HEV_PLUGIN_H__ */

