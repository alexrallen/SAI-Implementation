/*
 *  Copyright (C) 2017-2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License"); you may
 *    not use this file except in compliance with the License. You may obtain
 *    a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 *    THIS CODE IS PROVIDED ON AN  *AS IS* BASIS, WITHOUT WARRANTIES OR
 *    CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT
 *    LIMITATION ANY IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS
 *    FOR A PARTICULAR PURPOSE, MERCHANTABLITY OR NON-INFRINGEMENT.
 *
 *    See the Apache Version 2.0 License for specific language governing
 *    permissions and limitations under the License.
 *
 */

#include "sai_windows.h"
#include "sai.h"
#include "mlnx_sai.h"
#include "assert.h"
#include <fx_base_api.h>
#include <flextrum_types.h>
#include <sdk/sx_api_bmtor.h>

#undef  __MODULE__
#define __MODULE__ SAI_ROUTE

static sx_verbosity_level_t LOG_VAR_NAME(__MODULE__) = SX_VERBOSITY_LEVEL_WARNING;
static sai_status_t mlnx_route_packet_action_get(_In_ const sai_object_key_t   *key,
                                                 _Inout_ sai_attribute_value_t *value,
                                                 _In_ uint32_t                  attr_index,
                                                 _Inout_ vendor_cache_t        *cache,
                                                 void                          *arg);
static sai_status_t mlnx_route_trap_id_get(_In_ const sai_object_key_t   *key,
                                           _Inout_ sai_attribute_value_t *value,
                                           _In_ uint32_t                  attr_index,
                                           _Inout_ vendor_cache_t        *cache,
                                           void                          *arg);
static sai_status_t mlnx_route_next_hop_id_get(_In_ const sai_object_key_t   *key,
                                               _Inout_ sai_attribute_value_t *value,
                                               _In_ uint32_t                  attr_index,
                                               _Inout_ vendor_cache_t        *cache,
                                               void                          *arg);
static sai_status_t mlnx_route_counter_id_get(_In_ const sai_object_key_t   *key,
                                              _Inout_ sai_attribute_value_t *value,
                                              _In_ uint32_t                  attr_index,
                                              _Inout_ vendor_cache_t        *cache,
                                              void                          *arg);
static sai_status_t mlnx_route_packet_action_set(_In_ const sai_object_key_t      *key,
                                                 _In_ const sai_attribute_value_t *value,
                                                 void                             *arg);
static sai_status_t mlnx_route_trap_id_set(_In_ const sai_object_key_t      *key,
                                           _In_ const sai_attribute_value_t *value,
                                           void                             *arg);
static sai_status_t mlnx_route_next_hop_id_set(_In_ const sai_object_key_t      *key,
                                               _In_ const sai_attribute_value_t *value,
                                               void                             *arg);
static sai_status_t mlnx_get_route(const sai_route_entry_t* route_entry,
                                   sx_uc_route_get_entry_t *route_get_entry,
                                   sx_router_id_t          *vrid);
static sai_status_t mlnx_route_next_hop_id_get_ext(_In_ sx_ecmp_id_t ecmp, _Out_ sai_object_id_t *nh);
static sai_status_t mlnx_route_counter_id_set(_In_ const sai_object_key_t      *key,
                                              _In_ const sai_attribute_value_t *value,
                                              void                             *arg);
static const sai_vendor_attribute_entry_t route_vendor_attribs[] = {
    { SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION,
      { true, false, true, true },
      { true, false, true, true },
      mlnx_route_packet_action_get, NULL,
      mlnx_route_packet_action_set, NULL },
    { SAI_ROUTE_ENTRY_ATTR_USER_TRAP_ID,
      { false, false, false, false },
      { true, false, true, true },
      mlnx_route_trap_id_get, NULL,
      mlnx_route_trap_id_set, NULL },
    { SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID,
      { true, false, true, true },
      { true, false, true, true },
      mlnx_route_next_hop_id_get, NULL,
      mlnx_route_next_hop_id_set, NULL },
    { SAI_ROUTE_ENTRY_ATTR_COUNTER_ID,
      { true, false, true, true },
      { true, false, true, true },
      mlnx_route_counter_id_get, NULL,
      mlnx_route_counter_id_set, NULL },
    { SAI_ROUTE_ENTRY_ATTR_META_DATA,
      { false, false, false, false },
      { false, false, false, false },
      NULL, NULL,
      NULL, NULL },
    { END_FUNCTIONALITY_ATTRIBS_ID,
      { false, false, false, false },
      { false, false, false, false },
      NULL, NULL,
      NULL, NULL }
};
static const mlnx_attr_enum_info_t        route_enum_info[] = {
    [SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION] = ATTR_ENUM_VALUES_LIST(
        SAI_PACKET_ACTION_FORWARD,
        SAI_PACKET_ACTION_TRAP,
        SAI_PACKET_ACTION_LOG,
        SAI_PACKET_ACTION_DROP)
};
const mlnx_obj_type_attrs_info_t          mlnx_route_obj_type_info =
{ route_vendor_attribs, OBJ_ATTRS_ENUMS_INFO(route_enum_info), OBJ_STAT_CAP_INFO_EMPTY()};
static void route_key_to_str(_In_ const sai_route_entry_t* route_entry, _Out_ char *key_str)
{
    int res;

    res = snprintf(key_str, MAX_KEY_STR_LEN, "route ");
    sai_ipprefix_to_str(route_entry->destination, MAX_KEY_STR_LEN - res, key_str + res);
}

_Success_(return == SAI_STATUS_SUCCESS)
static sai_status_t mlnx_translate_sai_route_entry_to_sdk(_In_ const sai_route_entry_t *route_entry,
                                                          _Out_ sx_ip_prefix_t         *ip_prefix,
                                                          _Out_ sx_router_id_t         *vrid)
{
    uint32_t     data;
    sai_status_t status;

    if (SAI_STATUS_SUCCESS !=
        (status = mlnx_translate_sai_ip_prefix_to_sdk(&route_entry->destination, ip_prefix))) {
        return status;
    }

    if (SAI_STATUS_SUCCESS !=
        (status = mlnx_object_to_type(route_entry->vr_id, SAI_OBJECT_TYPE_VIRTUAL_ROUTER, &data, NULL))) {
        return status;
    }
    *vrid = (sx_router_id_t)data;

    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_route_handle_encap_nexthop(_In_ sai_object_id_t nh,
                                                    _In_ sai_object_id_t vrf,
                                                    _Out_ sx_ecmp_id_t  *sx_ecmp_id)
{
    sai_status_t                   status;
    mlnx_encap_nexthop_db_entry_t *db_entry;
    mlnx_shm_rm_array_idx_t        idx;

    assert(sx_ecmp_id);

    sai_db_write_lock();

    status = mlnx_encap_nexthop_oid_to_data(nh, &db_entry, &idx);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed get data from DB.\n");
        goto out;
    }

    status = mlnx_tunnel_bridge_counter_update(db_entry->data.tunnel_id,
                                               db_entry->data.tunnel_vni,
                                               vrf,
                                               1);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Update tunnel bridge counter failed.\n");
        goto out;
    }

    status = mlnx_encap_nexthop_counter_update(nh, vrf, 1);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Update encap nexthop counter failed.\n");
        goto out;
    }

    status = mlnx_encap_nexthop_get_ecmp(nh, vrf, sx_ecmp_id);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Get ecmp failed.\n");
        goto out;
    }

out:
    sai_db_unlock();
    return status;
}

static sai_status_t mlnx_fill_route_data(sx_uc_route_data_t      *route_data,
                                         sai_object_id_t          oid,
                                         uint32_t                 next_hop_param_index,
                                         const sai_route_entry_t *route_entry)
{
    sai_status_t  status;
    sx_ecmp_id_t  sdk_ecmp_id;
    sx_next_hop_t sdk_next_hop;
    uint32_t      sdk_next_hop_cnt;
    uint32_t      port_data;
    uint32_t      data;
    uint16_t      ext;

    SX_LOG_ENTER();

    if (SAI_OBJECT_TYPE_NEXT_HOP == sai_object_type_query(oid)) {
        if (SAI_STATUS_SUCCESS !=
            (status = mlnx_object_to_type(oid, SAI_OBJECT_TYPE_NEXT_HOP, &data, (uint8_t*)&ext))) {
            return status;
        }

        if (ext) {
            status = mlnx_route_handle_encap_nexthop(oid, route_entry->vr_id, &sdk_ecmp_id);
            if (SAI_ERR(status)) {
                SX_LOG_ERR("Fill route data (Encap Nexthop) failed.\n");
                return status;
            }
        } else {
            sdk_ecmp_id = (sx_ecmp_id_t)data;
        }

        route_data->type = SX_UC_ROUTE_TYPE_NEXT_HOP;
        route_data->uc_route_param.ecmp_id = sdk_ecmp_id;

        /* ECMP container should contains exactly 1 next hop */
        sdk_next_hop_cnt = 1;
        memset(&sdk_next_hop, 0, sizeof(sdk_next_hop));
        if (SX_STATUS_SUCCESS !=
            (status = sx_api_router_ecmp_get(gh_sdk, sdk_ecmp_id, &sdk_next_hop, &sdk_next_hop_cnt))) {
            SX_LOG_ERR("Failed to get ecmp - %s.\n", SX_STATUS_MSG(status));
            return sdk_to_sai(status);
        }
        if (1 != sdk_next_hop_cnt) {
            SX_LOG_ERR("Invalid next hop object\n");
            return SAI_STATUS_INVALID_ATTR_VALUE_0 + next_hop_param_index;
        }
    } else if (SAI_OBJECT_TYPE_NEXT_HOP_GROUP == sai_object_type_query(oid)) {
        if (SAI_STATUS_SUCCESS !=
            (status = mlnx_object_to_type(oid, SAI_OBJECT_TYPE_NEXT_HOP_GROUP, &sdk_ecmp_id, NULL))) {
            return status;
        }

        route_data->type = SX_UC_ROUTE_TYPE_NEXT_HOP;
        route_data->uc_route_param.ecmp_id = sdk_ecmp_id;
    } else if (SAI_OBJECT_TYPE_ROUTER_INTERFACE == sai_object_type_query(oid)) {
        if (SAI_STATUS_SUCCESS !=
            (status = mlnx_rif_oid_to_sdk_rif_id(oid, &route_data->uc_route_param.local_egress_rif))) {
            SX_LOG_ERR("Fail to get sdk rif id from rif oid %" PRIx64 "\n", oid);
            SX_LOG_EXIT();
            return status;
        }
        route_data->type = SX_UC_ROUTE_TYPE_LOCAL;
    } else if (SAI_OBJECT_TYPE_PORT == sai_object_type_query(oid)) {
        if (SAI_STATUS_SUCCESS !=
            (status = mlnx_object_to_type(oid, SAI_OBJECT_TYPE_PORT, &port_data, NULL))) {
            return status;
        }
        if (CPU_PORT != port_data) {
            SX_LOG_ERR("Invalid port passed as next hop id, only cpu port is valid - %u %u\n", port_data, CPU_PORT);
            return SAI_STATUS_INVALID_ATTR_VALUE_0 + next_hop_param_index;
        }
        route_data->type = SX_UC_ROUTE_TYPE_IP2ME;
    } else if (SAI_NULL_OBJECT_ID == oid) {
        route_data->type = SX_UC_ROUTE_TYPE_NEXT_HOP;
        if (SX_ROUTER_ACTION_TRAP != route_data->action) {
            route_data->action = SX_ROUTER_ACTION_DROP;
        }
        route_data->uc_route_param.ecmp_id = SX_ROUTER_ECMP_ID_INVALID;
    } else {
        SX_LOG_ERR("Invalid next hop object type - %s\n", SAI_TYPE_STR(sai_object_type_query(oid)));
        return SAI_STATUS_INVALID_ATTR_VALUE_0 + next_hop_param_index;
    }

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_route_attr_to_sx_data(_In_ const sai_route_entry_t *route_entry,
                                               _In_ const char              *key_str,
                                               _In_ uint32_t                 attr_count,
                                               _In_ const sai_attribute_t   *attr_list,
                                               _Out_ sx_ip_prefix_t         *sx_ip_prefix,
                                               _Out_ sx_router_id_t         *sx_vrid,
                                               _Out_ sx_uc_route_data_t     *sx_route_data)
{
    sai_status_t                 status;
    const sai_attribute_value_t *action, *next_hop;
    sai_object_id_t              next_hop_oid;
    uint32_t                     action_index, next_hop_index;
    char                         list_str[MAX_LIST_VALUE_STR_LEN];
    bool                         next_hop_id_found = false;
    sx_log_severity_t            log_level = SX_LOG_NOTICE;

    assert(sx_ip_prefix);
    assert(sx_vrid);
    assert(sx_route_data);

    if (NULL == route_entry) {
        SX_LOG_ERR("NULL route_entry param\n");
        return SAI_STATUS_INVALID_PARAMETER;
    }

    status = check_attribs_metadata(attr_count, attr_list, SAI_OBJECT_TYPE_ROUTE_ENTRY, route_vendor_attribs,
                                    SAI_COMMON_API_CREATE);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed attribs check\n");
        return status;
    }

    sai_attr_list_to_str(attr_count, attr_list, SAI_OBJECT_TYPE_ROUTE_ENTRY, MAX_LIST_VALUE_STR_LEN, list_str);
    /* lower log level for route created often in Sonic */
#ifdef ACS_OS
    log_level = SX_LOG_INFO;
#endif
    SX_LOG(log_level, "Create route %s\n", key_str);
    SX_LOG(log_level, "Attribs %s\n", list_str);

    sx_route_data->action = SX_ROUTER_ACTION_FORWARD;
    sx_route_data->trap_attr.prio = SX_TRAP_PRIORITY_MED;

    status = find_attrib_in_list(attr_count, attr_list, SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION, &action, &action_index);
    if (SAI_STATUS_SUCCESS == status) {
        status = mlnx_translate_sai_router_action_to_sdk(action->s32, &sx_route_data->action, action_index);
        if (SAI_ERR(status)) {
            return status;
        }
    }

    status = find_attrib_in_list(attr_count, attr_list, SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID, &next_hop, &next_hop_index);
    if (SAI_ERR(status)) {
        next_hop_oid = SAI_NULL_OBJECT_ID;
        next_hop_index = 0;
    } else {
        next_hop_oid = next_hop->oid;
        next_hop_id_found = true;
    }

    if (((SX_ROUTER_ACTION_FORWARD == sx_route_data->action) || (SX_ROUTER_ACTION_MIRROR == sx_route_data->action)) &&
        (!next_hop_id_found)) {
        SX_LOG_ERR(
            "Packet action forward/log without next hop / next hop group is not allowed for non directly reachable route\n");
        return SAI_STATUS_INVALID_PARAMETER;
    }

    status = mlnx_fill_route_data(sx_route_data, next_hop_oid, next_hop_index, route_entry);
    if (SAI_ERR(status)) {
        return status;
    }

    status = mlnx_translate_sai_route_entry_to_sdk(route_entry, sx_ip_prefix, sx_vrid);
    if (SAI_ERR(status)) {
        return status;
    }

    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_route_change_counter(sx_router_id_t  vrid,
                                              sx_ip_prefix_t *ip_prefix,
                                              sai_object_id_t counter_id)
{
    sx_flow_counter_id_t flow_counter;
    sx_status_t          sx_status;
    sai_status_t         status;

    if (SAI_NULL_OBJECT_ID != counter_id) {
        status = mlnx_get_flow_counter_id(counter_id, &flow_counter);
        if (SAI_ERR(status)) {
            SX_LOG_ERR("Failed to get flow counter\n");
            return status;
        }

        sx_status = sx_api_router_uc_route_counter_bind_set(gh_sdk, SX_ACCESS_CMD_BIND, vrid, ip_prefix, flow_counter);
    } else {
        sx_status = sx_api_router_uc_route_counter_bind_set(gh_sdk, SX_ACCESS_CMD_UNBIND, vrid, ip_prefix,
                                                            SX_FLOW_COUNTER_ID_INVALID);
    }

    if (SX_ERR(sx_status)) {
        SX_LOG_ERR("Failed to set counter - %s.\n", SX_STATUS_MSG(sx_status));
    }

    SX_LOG_EXIT();
    return sdk_to_sai(sx_status);
}

/*
 * Routine Description:
 *    Create Route
 *
 * Arguments:
 *    [in] route_entry - route entry
 *    [in] attr_count - number of attributes
 *    [in] attr_list - array of attributes
 *
 * Return Values:
 *    SAI_STATUS_SUCCESS on success
 *    Failure status code on error
 *
 * Note: IP prefix/mask expected in Network Byte Order.
 *
 */
static sai_status_t mlnx_create_route(_In_ const sai_route_entry_t* route_entry,
                                      _In_ uint32_t                 attr_count,
                                      _In_ const sai_attribute_t   *attr_list)
{
    sai_status_t                 status;
    sx_status_t                  sx_status;
    sx_ip_prefix_t               ip_prefix;
    sx_router_id_t               vrid = DEFAULT_VRID;
    sx_uc_route_data_t           route_data;
    char                         key_str[MAX_KEY_STR_LEN];
    sx_log_severity_t            log_level = SX_LOG_NOTICE;
    const sai_attribute_value_t *counter = NULL;
    uint32_t                     counter_index;

    SX_LOG_ENTER();

    memset(&ip_prefix, 0, sizeof(ip_prefix));
    memset(&route_data, 0, sizeof(route_data));

    route_key_to_str(route_entry, key_str);

    status = mlnx_route_attr_to_sx_data(route_entry, key_str, attr_count, attr_list, &ip_prefix, &vrid, &route_data);
    if (SAI_ERR(status)) {
        SX_LOG_EXIT();
        return status;
    }

    sx_status = sx_api_router_uc_route_set(gh_sdk, SX_ACCESS_CMD_ADD, vrid, &ip_prefix, &route_data);
    if (SX_ERR(sx_status)) {
        SX_LOG_ERR("Failed to set route - %s.\n", SX_STATUS_MSG(sx_status));
        SX_LOG_EXIT();
        return sdk_to_sai(sx_status);
    }

    status = find_attrib_in_list(attr_count, attr_list, SAI_ROUTE_ENTRY_ATTR_COUNTER_ID, &counter, &counter_index);
    if (!SAI_ERR(status)) {
        status = mlnx_route_change_counter(vrid, &ip_prefix, counter->oid);
        if (SAI_ERR(status)) {
            SX_LOG_EXIT();
            return status;
        }
    }

    /* lower log level for route created often in Sonic */
#ifdef ACS_OS
    log_level = SX_LOG_INFO;
#endif

    SX_LOG(log_level, "Created route %s\n", key_str);

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_route_to_encap_nexthop_remove(_In_ sai_object_id_t vrf, _In_ sai_object_id_t nh)
{
    sai_status_t                   status;
    mlnx_encap_nexthop_db_entry_t *db_entry;
    mlnx_shm_rm_array_idx_t        idx;

    sai_db_write_lock();

    status = mlnx_encap_nexthop_counter_update(nh, vrf, -1);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Update Encap Nexthop counter failed.\n");
        goto out;
    }

    status = mlnx_encap_nexthop_oid_to_data(nh, &db_entry, &idx);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed get data from DB.\n");
        goto out;
    }

    status = mlnx_tunnel_bridge_counter_update(db_entry->data.tunnel_id,
                                               db_entry->data.tunnel_vni,
                                               vrf,
                                               -1);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed to update Bridge counter.\n");
        goto out;
    }

out:
    sai_db_unlock();
    return status;
}

static sai_status_t mlnx_route_get_vrf_and_nh(_In_ const sai_route_entry_t *route_entry,
                                              _Out_ sai_object_id_t        *vrf,
                                              _Out_ sai_object_id_t        *nh,
                                              _Out_ bool                   *found)
{
    sai_status_t            status;
    sx_uc_route_get_entry_t route_get_entry;
    sx_router_id_t          vrid;
    uint32_t                data;
    uint16_t                use_db;

    status = mlnx_get_route(route_entry, &route_get_entry, &vrid);
    if (SAI_ERR(status)) {
        return status;
    }

    if ((SX_UC_ROUTE_TYPE_NEXT_HOP != route_get_entry.route_data.type) ||
        (SX_ROUTER_ECMP_ID_INVALID == route_get_entry.route_data.uc_route_param.ecmp_id)) {
        return SAI_STATUS_SUCCESS;
    }

    status = mlnx_route_next_hop_id_get_ext(route_get_entry.route_data.uc_route_param.ecmp_id,
                                            nh);
    if (SAI_ERR(status)) {
        return status;
    }

    if (SAI_OBJECT_TYPE_NEXT_HOP != sai_object_type_query(*nh)) {
        return SAI_STATUS_SUCCESS;
    }

    status = mlnx_object_to_type(*nh, SAI_OBJECT_TYPE_NEXT_HOP, &data, (uint8_t*)&use_db);
    if (SAI_ERR(status)) {
        return status;
    }

    if (!use_db) {
        return SAI_STATUS_SUCCESS;
    }

    *found = use_db;
    *vrf = route_entry->vr_id;

    return SAI_STATUS_SUCCESS;
}

/*
 * Routine Description:
 *    Remove Route
 *
 * Arguments:
 *    [in] route_entry - route entry
 *
 * Return Values:
 *    SAI_STATUS_SUCCESS on success
 *    Failure status code on error
 *
 * Note: IP prefix/mask expected in Network Byte Order.
 */
static sai_status_t mlnx_remove_route(_In_ const sai_route_entry_t* route_entry)
{
    sai_status_t    status;
    sx_status_t     sx_status;
    sx_ip_prefix_t  ip_prefix;
    char            key_str[MAX_KEY_STR_LEN];
    sx_router_id_t  vrid = DEFAULT_VRID;
    sai_object_id_t vrf;
    sai_object_id_t nh;
    bool            encap_nh_found = false;

    SX_LOG_ENTER();

    if (NULL == route_entry) {
        SX_LOG_ERR("NULL route_entry param\n");
        return SAI_STATUS_INVALID_PARAMETER;
    }

    route_key_to_str(route_entry, key_str);
    SX_LOG_NTC("Remove route %s\n", key_str);

    memset(&ip_prefix, 0, sizeof(ip_prefix));

    status = mlnx_translate_sai_route_entry_to_sdk(route_entry, &ip_prefix, &vrid);
    if (SAI_ERR(status)) {
        return status;
    }

    status = mlnx_route_get_vrf_and_nh(route_entry, &vrf, &nh, &encap_nh_found);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed to find Encap NH.\n");
        return status;
    }

    sx_status = sx_api_router_uc_route_set(gh_sdk, SX_ACCESS_CMD_DELETE, vrid, &ip_prefix, NULL);
    if (SX_ERR(sx_status)) {
        SX_LOG_ERR("Failed to remove route - %s.\n", SX_STATUS_MSG(sx_status));
        return sdk_to_sai(sx_status);
    }

    if (encap_nh_found) {
        status = mlnx_route_to_encap_nexthop_remove(vrf, nh);
        if (SAI_ERR(status)) {
            SX_LOG_ERR("Remove route to Encap Nexthop failed.\n");
            return status;
        }
    }

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

/*
 * Routine Description:
 *    Set route attribute value
 *
 * Arguments:
 *    [in] route_entry - route entry
 *    [in] attr - attribute
 *
 * Return Values:
 *    SAI_STATUS_SUCCESS on success
 *    Failure status code on error
 */
static sai_status_t mlnx_set_route_attribute(_In_ const sai_route_entry_t* route_entry,
                                             _In_ const sai_attribute_t   *attr)
{
    sai_object_key_t key;
    char             key_str[MAX_KEY_STR_LEN];

    SX_LOG_ENTER();

    if (NULL == route_entry) {
        SX_LOG_ERR("NULL route_entry param\n");
        return SAI_STATUS_INVALID_PARAMETER;
    }
    memcpy(&key.key.route_entry, route_entry, sizeof(*route_entry));

    route_key_to_str(route_entry, key_str);
    return sai_set_attribute(&key, key_str, SAI_OBJECT_TYPE_ROUTE_ENTRY, route_vendor_attribs, attr);
}

/*
 * Routine Description:
 *    Get route attribute value
 *
 * Arguments:
 *    [in] route_entry - route entry
 *    [in] attr_count - number of attributes
 *    [inout] attr_list - array of attributes
 *
 * Return Values:
 *    SAI_STATUS_SUCCESS on success
 *    Failure status code on error
 */
static sai_status_t mlnx_get_route_attribute(_In_ const sai_route_entry_t* route_entry,
                                             _In_ uint32_t                 attr_count,
                                             _Inout_ sai_attribute_t      *attr_list)
{
    sai_object_key_t key;
    char             key_str[MAX_KEY_STR_LEN];

    SX_LOG_ENTER();

    if (NULL == route_entry) {
        SX_LOG_ERR("NULL route_entry param\n");
        return SAI_STATUS_INVALID_PARAMETER;
    }
    memcpy(&key.key.route_entry, route_entry, sizeof(*route_entry));

    route_key_to_str(route_entry, key_str);
    return sai_get_attributes(&key, key_str, SAI_OBJECT_TYPE_ROUTE_ENTRY, route_vendor_attribs, attr_count, attr_list);
}

static sai_status_t mlnx_get_route(const sai_route_entry_t* route_entry,
                                   sx_uc_route_get_entry_t *route_get_entry,
                                   sx_router_id_t          *vrid)
{
    sx_status_t              status;
    uint32_t                 entries_count = 1;
    sx_ip_prefix_t           ip_prefix;
    sx_uc_route_key_filter_t filter;

    SX_LOG_ENTER();

    memset(&ip_prefix, 0, sizeof(ip_prefix));
    memset(&filter, 0, sizeof(filter));

    if (SAI_STATUS_SUCCESS !=
        (status = mlnx_translate_sai_route_entry_to_sdk(route_entry, &ip_prefix, vrid))) {
        return status;
    }

    if (SX_STATUS_SUCCESS !=
        (status =
             sx_api_router_uc_route_get(gh_sdk, SX_ACCESS_CMD_GET, *vrid, &ip_prefix, &filter,
                                        route_get_entry, &entries_count))) {
        SX_LOG_ERR("Failed to get %d route entries %s.\n", entries_count, SX_STATUS_MSG(status));
        return sdk_to_sai(status);
    }

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

/* Packet action [sai_packet_action_t] */
static sai_status_t mlnx_route_packet_action_get(_In_ const sai_object_key_t   *key,
                                                 _Inout_ sai_attribute_value_t *value,
                                                 _In_ uint32_t                  attr_index,
                                                 _Inout_ vendor_cache_t        *cache,
                                                 void                          *arg)
{
    sai_status_t             status;
    const sai_route_entry_t* route_entry = &key->key.route_entry;
    sx_uc_route_get_entry_t  route_get_entry;
    sx_router_id_t           vrid;

    SX_LOG_ENTER();

    if (SAI_STATUS_SUCCESS != (status = mlnx_get_route(route_entry, &route_get_entry, &vrid))) {
        return status;
    }

    if (SAI_STATUS_SUCCESS !=
        (status = mlnx_translate_sdk_router_action_to_sai(route_get_entry.route_data.action, &value->s32))) {
        return status;
    }

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_route_trap_id_get(_In_ const sai_object_key_t   *key,
                                           _Inout_ sai_attribute_value_t *value,
                                           _In_ uint32_t                  attr_index,
                                           _Inout_ vendor_cache_t        *cache,
                                           void                          *arg)
{
    return SAI_STATUS_NOT_IMPLEMENTED;
}

static sai_status_t mlnx_ecmp_get_ip(_In_ sx_ecmp_id_t sdk_ecmp_id, _Out_ sx_ip_addr_t *ip)
{
    sx_status_t   sx_status;
    sx_next_hop_t sdk_next_hop;
    uint32_t      sdk_next_hop_cnt;

    assert(ip);

    sdk_next_hop_cnt = 1;
    sx_status = sx_api_router_ecmp_get(gh_sdk, sdk_ecmp_id, &sdk_next_hop, &sdk_next_hop_cnt);
    if (SX_ERR(sx_status)) {
        SX_LOG_ERR("Failed to get ecmp - %s.\n", SX_STATUS_MSG(sx_status));
        return sdk_to_sai(sx_status);
    }

    if (1 != sdk_next_hop_cnt) {
        SX_LOG_DBG("Next hops count != 1, (value: %u)\n", sdk_next_hop_cnt);
        return SAI_STATUS_SUCCESS;
    }

    *ip = sdk_next_hop.next_hop_key.next_hop_key_entry.ip_next_hop.address;

    return SAI_STATUS_SUCCESS;
}

static bool mlnx_is_encap_nexthop_fake_ip(_In_ sx_ip_addr_t *ip)
{
    assert(ip);

    return (ip->version == SX_IP_VERSION_IPV4) &&
           (((ip->addr.ipv4.s_addr & 0xFF000000) >> 24) == 0) &&
           (((ip->addr.ipv4.s_addr & 0x00FFFF00) >> 8) < MAX_ENCAP_NEXTHOPS_NUMBER) &&
           (((ip->addr.ipv4.s_addr & 0x000000FF) < NUMBER_OF_LOCAL_VNETS));
}

static sai_status_t mlnx_route_get_encap_nexthop(_In_ sx_ip_addr_t *ip, _Out_ sai_object_id_t *nh)
{
    sai_status_t            status;
    mlnx_shm_rm_array_idx_t db_idx;

    db_idx.idx = (ip->addr.ipv4.s_addr & 0x00FFFF00) >> 8;
    db_idx.type = MLNX_SHM_RM_ARRAY_TYPE_NEXTHOP;

    sai_db_write_lock();
    status = mlnx_encap_nexthop_oid_create(db_idx, nh);
    sai_db_unlock();

    return status;
}

static sai_status_t mlnx_route_next_hop_id_get_ext(_In_ sx_ecmp_id_t ecmp, _Out_ sai_object_id_t *nh)
{
    sai_status_t status;
    sx_ip_addr_t ip = {0};

    assert(nh);

    status = mlnx_ecmp_get_ip(ecmp, &ip);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Get ECMP IP failed.\n");
        return status;
    }

    if (mlnx_is_encap_nexthop_fake_ip(&ip)) {
        status = mlnx_route_get_encap_nexthop(&ip, nh);
        if (SAI_ERR(status)) {
            SX_LOG_ERR("Get Encap Next Hop OID failed.\n");
            return status;
        }
    } else {
        status = mlnx_create_object(SAI_OBJECT_TYPE_NEXT_HOP_GROUP,
                                    ecmp,
                                    NULL,
                                    nh);
        if (SAI_ERR(status)) {
            SX_LOG_ERR("Create Next Hop OID failed.\n");
            return status;
        }
    }

    return SAI_STATUS_SUCCESS;
}

/* Next hop or next hop group id for the packet or a router interface
 * in case of directly reachable route [sai_object_id_t]
 * The next hop id can be a generic next hop object, such as next hop,
 * next hop group.
 * Directly reachable routes are the IP subnets that are directly attached to the router.
 * For such routes, fill the router interface id to which the subnet is attached */
static sai_status_t mlnx_route_next_hop_id_get(_In_ const sai_object_key_t   *key,
                                               _Inout_ sai_attribute_value_t *value,
                                               _In_ uint32_t                  attr_index,
                                               _Inout_ vendor_cache_t        *cache,
                                               void                          *arg)
{
    sai_status_t             status;
    const sai_route_entry_t *route_entry = &key->key.route_entry;
    sx_uc_route_get_entry_t  route_get_entry;
    sx_router_id_t           vrid;

    SX_LOG_ENTER();

    status = mlnx_get_route(route_entry, &route_get_entry, &vrid);
    if (SAI_ERR(status)) {
        return status;
    }

    if (SX_UC_ROUTE_TYPE_LOCAL == route_get_entry.route_data.type) {
        status = mlnx_rif_sx_to_sai_oid(route_get_entry.route_data.uc_route_param.
                                        local_egress_rif,
                                        &value->oid);
        if (SAI_ERR(status)) {
            return status;
        }
    } else if (SX_UC_ROUTE_TYPE_NEXT_HOP == route_get_entry.route_data.type) {
        if (SX_ROUTER_ECMP_ID_INVALID != route_get_entry.route_data.uc_route_param.ecmp_id) {
            status = mlnx_route_next_hop_id_get_ext(route_get_entry.route_data.uc_route_param.ecmp_id,
                                                    &value->oid);
            if (SAI_ERR(status)) {
                return status;
            }
        } else {
            value->oid = SAI_NULL_OBJECT_ID;
        }
    } else if (SX_UC_ROUTE_TYPE_IP2ME == route_get_entry.route_data.type) {
        status = mlnx_create_object(SAI_OBJECT_TYPE_PORT, CPU_PORT, NULL, &value->oid);
        if (SAI_ERR(status)) {
            return status;
        }
    } else {
        SX_LOG_ERR("Unexpected sx route type %u\n", route_get_entry.route_data.type);
        return SAI_STATUS_FAILURE;
    }

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_modify_route(sx_router_id_t           vrid,
                                      sx_uc_route_get_entry_t *route_get_entry,
                                      sx_access_cmd_t          cmd)
{
    sx_status_t status;

    /* Delete and Add for action/priority, or Set for next hops changes */
    if (SX_ACCESS_CMD_ADD == cmd) {
        if (SX_STATUS_SUCCESS !=
            (status = sx_api_router_uc_route_set(gh_sdk, SX_ACCESS_CMD_DELETE, vrid,
                                                 &route_get_entry->network_addr, &route_get_entry->route_data))) {
            SX_LOG_ERR("Failed to delete route - %s.\n", SX_STATUS_MSG(status));
            return sdk_to_sai(status);
        }
    }

    if (SX_STATUS_SUCCESS !=
        (status = sx_api_router_uc_route_set(gh_sdk, cmd, vrid,
                                             &route_get_entry->network_addr, &route_get_entry->route_data))) {
        SX_LOG_ERR("Failed to set route - %s.\n", SX_STATUS_MSG(status));
        return sdk_to_sai(status);
    }

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

/* Packet action [sai_packet_action_t] */
static sai_status_t mlnx_route_packet_action_set(_In_ const sai_object_key_t      *key,
                                                 _In_ const sai_attribute_value_t *value,
                                                 void                             *arg)
{
    sai_status_t             status;
    const sai_route_entry_t* route_entry = &key->key.route_entry;
    sx_uc_route_get_entry_t  route_get_entry;
    sx_router_action_t       route_action;
    sx_router_id_t           vrid;

    SX_LOG_ENTER();

    if (SAI_STATUS_SUCCESS != (status = mlnx_get_route(route_entry, &route_get_entry, &vrid))) {
        return status;
    }

    route_action = route_get_entry.route_data.action;
    if (((SX_ROUTER_ACTION_DROP == route_action) || (SX_ROUTER_ACTION_TRAP == route_action)) &&
        ((SAI_PACKET_ACTION_FORWARD == value->s32) || (SAI_PACKET_ACTION_LOG == value->s32))) {
        status = mlnx_fdb_route_action_save(SAI_OBJECT_TYPE_ROUTE_ENTRY, route_entry, value->s32);
        if (SAI_ERR(status)) {
            return status;
        }
    } else {
        if (SAI_STATUS_SUCCESS !=
            (status = mlnx_translate_sai_router_action_to_sdk(value->s32, &route_get_entry.route_data.action, 0))) {
            return status;
        }

        mlnx_fdb_route_action_clear(SAI_OBJECT_TYPE_ROUTE_ENTRY, route_entry);

        if (SAI_STATUS_SUCCESS != (status = mlnx_modify_route(vrid, &route_get_entry, SX_ACCESS_CMD_ADD))) {
            return status;
        }
    }

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_route_trap_id_set(_In_ const sai_object_key_t      *key,
                                           _In_ const sai_attribute_value_t *value,
                                           void                             *arg)
{
    return SAI_STATUS_NOT_IMPLEMENTED;
}

/* Next hop or next hop group id for the packet or a router interface
 * in case of directly reachable route [sai_object_id_t]
 * The next hop id can be a generic next hop object, such as next hop,
 * next hop group.
 * Directly reachable routes are the IP subnets that are directly attached to the router.
 * For such routes, fill the router interface id to which the subnet is attached */
static sai_status_t mlnx_route_next_hop_id_set(_In_ const sai_object_key_t      *key,
                                               _In_ const sai_attribute_value_t *value,
                                               void                             *arg)
{
    sai_status_t             status;
    const sai_route_entry_t* route_entry = &key->key.route_entry;
    sx_uc_route_get_entry_t  route_get_entry;
    sx_router_id_t           vrid;
    sai_object_id_t          vrf;
    sai_object_id_t          nh;
    bool                     encap_nh_found = false;

    SX_LOG_ENTER();

    status = mlnx_get_route(route_entry, &route_get_entry, &vrid);
    if (SAI_ERR(status)) {
        return status;
    }

    mlnx_fdb_route_action_fetch(SAI_OBJECT_TYPE_ROUTE_ENTRY, route_entry, &route_get_entry.route_data.action);

    status = mlnx_route_get_vrf_and_nh(route_entry, &vrf, &nh, &encap_nh_found);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed to find Encap NH.\n");
        return status;
    }

    status = mlnx_fill_route_data(&route_get_entry.route_data, value->oid, 0, route_entry);
    if (SAI_ERR(status)) {
        return status;
    }

    status = mlnx_modify_route(vrid, &route_get_entry, SX_ACCESS_CMD_SET);
    if (SAI_ERR(status)) {
        return status;
    }

    if (encap_nh_found) {
        status = mlnx_route_to_encap_nexthop_remove(vrf, nh);
        if (SAI_ERR(status)) {
            SX_LOG_ERR("Remove route to Encap Nexthop failed.\n");
            return status;
        }
    }

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_route_counter_id_set(_In_ const sai_object_key_t      *key,
                                              _In_ const sai_attribute_value_t *value,
                                              void                             *arg)
{
    sai_status_t             status;
    const sai_route_entry_t* route_entry = &key->key.route_entry;
    sx_uc_route_get_entry_t  route_get_entry;
    sx_router_id_t           vrid;

    SX_LOG_ENTER();
    status = mlnx_get_route(route_entry, &route_get_entry, &vrid);
    if (SAI_ERR(status)) {
        return status;
    }

    return mlnx_route_change_counter(vrid, &route_get_entry.network_addr, value->oid);
}

static sai_status_t mlnx_route_counter_id_get(_In_ const sai_object_key_t   *key,
                                              _Inout_ sai_attribute_value_t *value,
                                              _In_ uint32_t                  attr_index,
                                              _Inout_ vendor_cache_t        *cache,
                                              void                          *arg)
{
    sai_status_t             status;
    const sai_route_entry_t* route_entry = &key->key.route_entry;
    sx_uc_route_get_entry_t  route_get_entry;
    sx_router_id_t           vrid;
    sx_status_t              sx_status;
    sx_flow_counter_id_t     flow_counter;

    SX_LOG_ENTER();

    if (SAI_STATUS_SUCCESS != (status = mlnx_get_route(route_entry, &route_get_entry, &vrid))) {
        return status;
    }

    sx_status = sx_api_router_uc_route_counter_bind_get(gh_sdk, vrid, &route_get_entry.network_addr, &flow_counter);
    if (SX_ERR(sx_status)) {
        SX_LOG_ERR("Failed to get route counter - %s\n", SX_STATUS_MSG(sx_status));
        return sdk_to_sai(sx_status);
    }

    status = mlnx_translate_flow_counter_to_sai_counter(flow_counter, &value->oid);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed to translate flow counter to SAI  counter\n");
        return status;
    }

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

sai_status_t mlnx_route_log_set(sx_verbosity_level_t level)
{
    LOG_VAR_NAME(__MODULE__) = level;

    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_route_bulk_api_impl(_In_ sai_common_api_t         api,
                                             _In_ uint32_t                 object_count,
                                             _In_ const sai_route_entry_t *route_entry,
                                             _In_ const uint32_t          *attr_count,
                                             _In_ const sai_attribute_t  **attr_list_for_create,
                                             _In_ sai_attribute_t        **attr_list_for_get,
                                             _In_ const sai_attribute_t   *attr_list_for_set,
                                             _In_ sai_bulk_op_error_mode_t mode,
                                             _Out_ sai_status_t           *object_statuses)
{
    sai_status_t status;
    uint32_t     ii;
    bool         stop_on_error, failure = false;

    SX_LOG_ENTER();

    assert((api == SAI_COMMON_API_BULK_CREATE) || (api == SAI_COMMON_API_BULK_REMOVE) ||
           (api == SAI_COMMON_API_BULK_GET) || (api == SAI_COMMON_API_BULK_SET));

    status = mlnx_bulk_attrs_validate(object_count, attr_count, attr_list_for_create, attr_list_for_get,
                                      attr_list_for_set, mode, object_statuses, api, &stop_on_error);
    if (SAI_ERR(status)) {
        return status;
    }

    if (!route_entry) {
        SX_LOG_ERR("route_entry is NULL");
        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (stop_on_error) {
        for (ii = 0; ii < object_count; ii++) {
            object_statuses[ii] = SAI_STATUS_NOT_EXECUTED;
        }
    }

    for (ii = 0; ii < object_count; ii++) {
        switch (api) {
        case SAI_COMMON_API_BULK_CREATE:
            object_statuses[ii] = mlnx_create_route(&route_entry[ii], attr_count[ii], attr_list_for_create[ii]);
            break;

        case SAI_COMMON_API_BULK_GET:
            object_statuses[ii] = mlnx_get_route_attribute(&route_entry[ii], attr_count[ii], attr_list_for_get[ii]);
            break;

        case SAI_COMMON_API_BULK_SET:
            object_statuses[ii] = mlnx_set_route_attribute(&route_entry[ii], &attr_list_for_set[ii]);
            break;

        case SAI_COMMON_API_BULK_REMOVE:
            object_statuses[ii] = mlnx_remove_route(&route_entry[ii]);
            break;

        default:
            assert(false);
        }

        if (SAI_ERR(object_statuses[ii])) {
            failure = true;
            if (stop_on_error) {
                goto out;
            } else {
                continue;
            }
        }
    }

out:
    mlnx_bulk_statuses_print("Routes", object_statuses, object_count, api);
    SX_LOG_EXIT();
    return failure ? SAI_STATUS_FAILURE : SAI_STATUS_SUCCESS;
}

/**
 * @brief Bulk create route entry
 *
 * @param[in] object_count Number of objects to create
 * @param[in] route_entry List of object to create
 * @param[in] attr_count List of attr_count. Caller passes the number
 *    of attribute for each object to create.
 * @param[in] attr_list List of attributes for every object.
 * @param[in] mode Bulk operation error handling mode.
 * @param[out] object_statuses List of status for every object. Caller needs to
 * allocate the buffer
 *
 * @return #SAI_STATUS_SUCCESS on success when all objects are created or
 * #SAI_STATUS_FAILURE when any of the objects fails to create. When there is
 * failure, Caller is expected to go through the list of returned statuses to
 * find out which fails and which succeeds.
 */
static sai_status_t mlnx_bulk_create_route_entry(_In_ uint32_t                 object_count,
                                                 _In_ const sai_route_entry_t *route_entry,
                                                 _In_ const uint32_t          *attr_count,
                                                 _In_ const sai_attribute_t  **attr_list,
                                                 _In_ sai_bulk_op_error_mode_t mode,
                                                 _Out_ sai_status_t           *object_statuses)
{
    return mlnx_route_bulk_api_impl(SAI_COMMON_API_BULK_CREATE, object_count, route_entry, attr_count,
                                    attr_list, NULL, NULL, mode, object_statuses);
}

/**
 * @brief Bulk remove route entry
 *
 * @param[in] object_count Number of objects to remove
 * @param[in] route_entry List of objects to remove
 * @param[in] mode Bulk operation error handling mode.
 * @param[out] object_statuses List of status for every object. Caller needs to
 * allocate the buffer
 *
 * @return #SAI_STATUS_SUCCESS on success when all objects are removed or
 * #SAI_STATUS_FAILURE when any of the objects fails to remove. When there is
 * failure, Caller is expected to go through the list of returned statuses to
 * find out which fails and which succeeds.
 */
static sai_status_t mlnx_bulk_remove_route_entry(_In_ uint32_t                 object_count,
                                                 _In_ const sai_route_entry_t *route_entry,
                                                 _In_ sai_bulk_op_error_mode_t mode,
                                                 _Out_ sai_status_t           *object_statuses)
{
    return mlnx_route_bulk_api_impl(SAI_COMMON_API_BULK_REMOVE, object_count, route_entry, NULL,
                                    NULL, NULL, NULL, mode, object_statuses);
}

/**
 * @brief Bulk set attribute on route entry
 *
 * @param[in] object_count Number of objects to set attribute
 * @param[in] route_entry List of objects to set attribute
 * @param[in] attr_list List of attributes to set on objects, one attribute per object
 * @param[in] mode Bulk operation error handling mode.
 * @param[out] object_statuses List of status for every object. Caller needs to
 * allocate the buffer
 *
 * @return #SAI_STATUS_SUCCESS on success when all objects are removed or
 * #SAI_STATUS_FAILURE when any of the objects fails to remove. When there is
 * failure, Caller is expected to go through the list of returned statuses to
 * find out which fails and which succeeds.
 */
static sai_status_t mlnx_bulk_set_route_entry_attribute(_In_ uint32_t                 object_count,
                                                        _In_ const sai_route_entry_t *route_entry,
                                                        _In_ const sai_attribute_t   *attr_list,
                                                        _In_ sai_bulk_op_error_mode_t mode,
                                                        _Out_ sai_status_t           *object_statuses)
{
    return mlnx_route_bulk_api_impl(SAI_COMMON_API_BULK_SET, object_count, route_entry, NULL, NULL, NULL,
                                    attr_list, mode, object_statuses);
}

/**
 * @brief Bulk get attribute on route entry
 *
 * @param[in] object_count Number of objects to set attribute
 * @param[in] route_entry List of objects to set attribute
 * @param[in] attr_count List of attr_count. Caller passes the number
 *    of attribute for each object to get
 * @param[inout] attr_list List of attributes to set on objects, one attribute per object
 * @param[in] mode Bulk operation error handling mode
 * @param[out] object_statuses List of status for every object. Caller needs to
 * allocate the buffer
 *
 * @return #SAI_STATUS_SUCCESS on success when all objects are removed or
 * #SAI_STATUS_FAILURE when any of the objects fails to remove. When there is
 * failure, Caller is expected to go through the list of returned statuses to
 * find out which fails and which succeeds.
 */
static sai_status_t mlnx_bulk_get_route_entry_attribute(_In_ uint32_t                 object_count,
                                                        _In_ const sai_route_entry_t *route_entry,
                                                        _In_ const uint32_t          *attr_count,
                                                        _Inout_ sai_attribute_t     **attr_list,
                                                        _In_ sai_bulk_op_error_mode_t mode,
                                                        _Out_ sai_status_t           *object_statuses)
{
    return mlnx_route_bulk_api_impl(SAI_COMMON_API_BULK_GET, object_count, route_entry, attr_count, NULL, attr_list,
                                    NULL, mode, object_statuses);
}

static sai_status_t mlnx_bmtor_rif_event(_In_ sx_router_interface_t sx_rif, bool is_add)
{
    sai_status_t status;
    sx_status_t  sx_status;
    fx_handle_t *p_fx_handle = NULL;

    /* Check if initialized */
    if (SX_STATUS_SUCCESS != (status = sdk_to_sai(fx_default_handle_get(&p_fx_handle, &gh_sdk, 0)))) {
        SX_LOG_ERR("Failure in obtaining the default fx_handle\n");
        return SAI_STATUS_FAILURE;
    }

    if (!*p_fx_handle) {
        SX_LOG_DBG("FX handle wasn't initialized\n");
        return SAI_STATUS_SUCCESS;
    }

    SX_LOG_DBG("bmtor event: %s rif %d\n", is_add ? "adding" : "removing", sx_rif);

    sx_status = fx_pipe_binding_update(*p_fx_handle, FX_CONTROL_OUT_RIF, &sx_rif, is_add);
    if (SX_ERR(sx_status)) {
        SX_LOG_ERR("Failed to update fx mapping\n");
        return sdk_to_sai(sx_status);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t mlnx_bmtor_rif_event_add(_In_ sx_router_interface_t sx_rif)
{
    return mlnx_bmtor_rif_event(sx_rif, true);
}

sai_status_t mlnx_bmtor_rif_event_del(_In_ sx_router_interface_t sx_rif)
{
    return mlnx_bmtor_rif_event(sx_rif, false);
}

sai_status_t sai_fx_uninitialize(void)
{
    return sdk_to_sai(fx_default_handle_free());
}

const sai_route_api_t mlnx_route_api = {
    mlnx_create_route,
    mlnx_remove_route,
    mlnx_set_route_attribute,
    mlnx_get_route_attribute,
    mlnx_bulk_create_route_entry,
    mlnx_bulk_remove_route_entry,
    mlnx_bulk_set_route_entry_attribute,
    mlnx_bulk_get_route_entry_attribute
};
