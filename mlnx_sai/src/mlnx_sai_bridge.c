/*
 *  Copyright (C) 2014-2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
#include "mlnx_sai.h"
#include "assert.h"
#include "sai.h"
#include <sx/sdk/sx_api_rm.h>

#undef  __MODULE__
#define __MODULE__ SAI_BRIDGE

static sx_verbosity_level_t LOG_VAR_NAME(__MODULE__) = SX_VERBOSITY_LEVEL_WARNING;
static sai_status_t mlnx_bridge_type_get(_In_ const sai_object_key_t   *key,
                                         _Inout_ sai_attribute_value_t *value,
                                         _In_ uint32_t                  attr_index,
                                         _Inout_ vendor_cache_t        *cache,
                                         void                          *arg);
static sai_status_t mlnx_bridge_port_list_get(_In_ const sai_object_key_t   *key,
                                              _Inout_ sai_attribute_value_t *value,
                                              _In_ uint32_t                  attr_index,
                                              _Inout_ vendor_cache_t        *cache,
                                              void                          *arg);
static sai_status_t mlnx_bridge_max_learned_addresses_get(_In_ const sai_object_key_t   *key,
                                                          _Inout_ sai_attribute_value_t *value,
                                                          _In_ uint32_t                  attr_index,
                                                          _Inout_ vendor_cache_t        *cache,
                                                          void                          *arg);
static sai_status_t mlnx_bridge_max_learned_addresses_set(_In_ const sai_object_key_t      *key,
                                                          _In_ const sai_attribute_value_t *value,
                                                          void                             *arg);
static sai_status_t mlnx_bridge_learn_disable_get(_In_ const sai_object_key_t   *key,
                                                  _Inout_ sai_attribute_value_t *value,
                                                  _In_ uint32_t                  attr_index,
                                                  _Inout_ vendor_cache_t        *cache,
                                                  void                          *arg);
static sai_status_t mlnx_bridge_learn_disable_set_impl(_In_ sx_bridge_id_t sx_bridge_id, _In_ bool learn_disable);
static sai_status_t mlnx_bridge_learn_disable_set(_In_ const sai_object_key_t      *key,
                                                  _In_ const sai_attribute_value_t *value,
                                                  void                             *arg);
static sai_status_t mlnx_bridge_flood_ctrl_get(_In_ const sai_object_key_t   *key,
                                               _Inout_ sai_attribute_value_t *value,
                                               _In_ uint32_t                  attr_index,
                                               _Inout_ vendor_cache_t        *cache,
                                               void                          *arg);
static sai_status_t mlnx_bridge_flood_ctrl_set(_In_ const sai_object_key_t      *key,
                                               _In_ const sai_attribute_value_t *value,
                                               void                             *arg);
static sai_status_t mlnx_bridge_flood_group_get(_In_ const sai_object_key_t   *key,
                                                _Inout_ sai_attribute_value_t *value,
                                                _In_ uint32_t                  attr_index,
                                                _Inout_ vendor_cache_t        *cache,
                                                void                          *arg);
static sai_status_t mlnx_bridge_flood_group_set(_In_ const sai_object_key_t      *key,
                                                _In_ const sai_attribute_value_t *value,
                                                void                             *arg);
static sai_status_t mlnx_bridge_sx_to_db_idx(_In_ sx_bridge_id_t sx_bridge_id, _Out_ mlnx_shm_rm_array_idx_t *idx);
static sai_status_t mlnx_bridge_1d_oid_to_data(_In_ sai_object_id_t           bridge_oid,
                                               _Out_ mlnx_bridge_t          **bridge,
                                               _Out_ mlnx_shm_rm_array_idx_t *idx);
static sai_status_t mlnx_sdk_bridge_create(_Out_ sx_bridge_id_t *sx_bridge_id);
static const sai_vendor_attribute_entry_t bridge_vendor_attribs[] = {
    { SAI_BRIDGE_ATTR_TYPE,
      { true, false, false, true },
      { true, false, false, true },
      mlnx_bridge_type_get, NULL,
      NULL, NULL },
    { SAI_BRIDGE_ATTR_PORT_LIST,
      { false, false, false, true },
      { false, false, false, true },
      mlnx_bridge_port_list_get, NULL,
      NULL, NULL },
    { SAI_BRIDGE_ATTR_MAX_LEARNED_ADDRESSES,
      { true, false, true, true },
      { true, false, true, true },
      mlnx_bridge_max_learned_addresses_get, NULL,
      mlnx_bridge_max_learned_addresses_set, NULL },
    { SAI_BRIDGE_ATTR_LEARN_DISABLE,
      { true, false, true, true },
      { true, false, true, true },
      mlnx_bridge_learn_disable_get, NULL,
      mlnx_bridge_learn_disable_set, NULL },
    { SAI_BRIDGE_ATTR_UNKNOWN_UNICAST_FLOOD_CONTROL_TYPE,
      { true, false, true, true },
      { true, false, true, true },
      mlnx_bridge_flood_ctrl_get, (void*)SAI_BRIDGE_ATTR_UNKNOWN_UNICAST_FLOOD_CONTROL_TYPE,
      mlnx_bridge_flood_ctrl_set, (void*)SAI_BRIDGE_ATTR_UNKNOWN_UNICAST_FLOOD_CONTROL_TYPE },
    { SAI_BRIDGE_ATTR_UNKNOWN_UNICAST_FLOOD_GROUP,
      { true, false, true, true },
      { true, false, true, true },
      mlnx_bridge_flood_group_get, (void*)SAI_BRIDGE_ATTR_UNKNOWN_UNICAST_FLOOD_GROUP,
      mlnx_bridge_flood_group_set, (void*)SAI_BRIDGE_ATTR_UNKNOWN_UNICAST_FLOOD_GROUP },
    { SAI_BRIDGE_ATTR_UNKNOWN_MULTICAST_FLOOD_CONTROL_TYPE,
      { true, false, true, true },
      { true, false, true, true },
      mlnx_bridge_flood_ctrl_get, (void*)SAI_BRIDGE_ATTR_UNKNOWN_MULTICAST_FLOOD_CONTROL_TYPE,
      mlnx_bridge_flood_ctrl_set, (void*)SAI_BRIDGE_ATTR_UNKNOWN_MULTICAST_FLOOD_CONTROL_TYPE},
    { SAI_BRIDGE_ATTR_UNKNOWN_MULTICAST_FLOOD_GROUP,
      { true, false, true, true },
      { true, false, true, true },
      mlnx_bridge_flood_group_get, (void*)SAI_BRIDGE_ATTR_UNKNOWN_MULTICAST_FLOOD_GROUP,
      mlnx_bridge_flood_group_set, (void*)SAI_BRIDGE_ATTR_UNKNOWN_MULTICAST_FLOOD_GROUP },
    { SAI_BRIDGE_ATTR_BROADCAST_FLOOD_CONTROL_TYPE,
      { true, false, true, true },
      { true, false, true, true },
      mlnx_bridge_flood_ctrl_get, (void*)SAI_BRIDGE_ATTR_BROADCAST_FLOOD_CONTROL_TYPE,
      mlnx_bridge_flood_ctrl_set, (void*)SAI_BRIDGE_ATTR_BROADCAST_FLOOD_CONTROL_TYPE },
    { SAI_BRIDGE_ATTR_BROADCAST_FLOOD_GROUP,
      { true, false, true, true },
      { true, false, true, true },
      mlnx_bridge_flood_group_get, (void*)SAI_BRIDGE_ATTR_BROADCAST_FLOOD_GROUP,
      mlnx_bridge_flood_group_set, (void*)SAI_BRIDGE_ATTR_BROADCAST_FLOOD_GROUP },
    { END_FUNCTIONALITY_ATTRIBS_ID,
      { false, false, false, false },
      { false, false, false, false },
      NULL, NULL,
      NULL, NULL }
};
static const mlnx_attr_enum_info_t        bridge_enum_info[] = {
    [SAI_BRIDGE_ATTR_TYPE] = ATTR_ENUM_VALUES_ALL(),
    [SAI_BRIDGE_ATTR_UNKNOWN_UNICAST_FLOOD_CONTROL_TYPE] = ATTR_ENUM_VALUES_LIST(
        SAI_BRIDGE_FLOOD_CONTROL_TYPE_SUB_PORTS,
        SAI_BRIDGE_FLOOD_CONTROL_TYPE_L2MC_GROUP,
        SAI_BRIDGE_FLOOD_CONTROL_TYPE_NONE),
    [SAI_BRIDGE_ATTR_UNKNOWN_MULTICAST_FLOOD_CONTROL_TYPE] = ATTR_ENUM_VALUES_LIST(
        SAI_BRIDGE_FLOOD_CONTROL_TYPE_SUB_PORTS,
        SAI_BRIDGE_FLOOD_CONTROL_TYPE_L2MC_GROUP,
        SAI_BRIDGE_FLOOD_CONTROL_TYPE_NONE),
    [SAI_BRIDGE_ATTR_BROADCAST_FLOOD_CONTROL_TYPE] = ATTR_ENUM_VALUES_LIST(
        SAI_BRIDGE_FLOOD_CONTROL_TYPE_SUB_PORTS,
        SAI_BRIDGE_FLOOD_CONTROL_TYPE_L2MC_GROUP,
        SAI_BRIDGE_FLOOD_CONTROL_TYPE_NONE),
};
const mlnx_obj_type_attrs_info_t          mlnx_bridge_obj_type_info =
{ bridge_vendor_attribs, OBJ_ATTRS_ENUMS_INFO(bridge_enum_info), OBJ_STAT_CAP_INFO_EMPTY()};
static sai_status_t mlnx_bridge_port_type_get(_In_ const sai_object_key_t   *key,
                                              _Inout_ sai_attribute_value_t *value,
                                              _In_ uint32_t                  attr_index,
                                              _Inout_ vendor_cache_t        *cache,
                                              void                          *arg);
static sai_status_t mlnx_bridge_port_lag_or_port_get(_In_ const sai_object_key_t   *key,
                                                     _Inout_ sai_attribute_value_t *value,
                                                     _In_ uint32_t                  attr_index,
                                                     _Inout_ vendor_cache_t        *cache,
                                                     void                          *arg);
static sai_status_t mlnx_bridge_port_vlan_id_get(_In_ const sai_object_key_t   *key,
                                                 _Inout_ sai_attribute_value_t *value,
                                                 _In_ uint32_t                  attr_index,
                                                 _Inout_ vendor_cache_t        *cache,
                                                 void                          *arg);
static sai_status_t mlnx_bridge_port_rif_id_get(_In_ const sai_object_key_t   *key,
                                                _Inout_ sai_attribute_value_t *value,
                                                _In_ uint32_t                  attr_index,
                                                _Inout_ vendor_cache_t        *cache,
                                                void                          *arg);
static sai_status_t mlnx_bridge_port_tunnel_id_get(_In_ const sai_object_key_t   *key,
                                                   _Inout_ sai_attribute_value_t *value,
                                                   _In_ uint32_t                  attr_index,
                                                   _Inout_ vendor_cache_t        *cache,
                                                   void                          *arg);
static sai_status_t mlnx_bridge_port_bridge_id_get(_In_ const sai_object_key_t   *key,
                                                   _Inout_ sai_attribute_value_t *value,
                                                   _In_ uint32_t                  attr_index,
                                                   _Inout_ vendor_cache_t        *cache,
                                                   void                          *arg);
static sai_status_t mlnx_bridge_port_bridge_id_set(_In_ const sai_object_key_t      *key,
                                                   _In_ const sai_attribute_value_t *value,
                                                   void                             *arg);
static sai_status_t mlnx_bridge_port_fdb_learning_mode_get(_In_ const sai_object_key_t   *key,
                                                           _Inout_ sai_attribute_value_t *value,
                                                           _In_ uint32_t                  attr_index,
                                                           _Inout_ vendor_cache_t        *cache,
                                                           void                          *arg);
static sai_status_t mlnx_bridge_port_fdb_learning_mode_set(_In_ const sai_object_key_t      *key,
                                                           _In_ const sai_attribute_value_t *value,
                                                           void                             *arg);
static sai_status_t mlnx_bridge_port_max_learned_addresses_get(_In_ const sai_object_key_t   *key,
                                                               _Inout_ sai_attribute_value_t *value,
                                                               _In_ uint32_t                  attr_index,
                                                               _Inout_ vendor_cache_t        *cache,
                                                               void                          *arg);
static sai_status_t mlnx_bridge_port_max_learned_addresses_set(_In_ const sai_object_key_t      *key,
                                                               _In_ const sai_attribute_value_t *value,
                                                               void                             *arg);
static sai_status_t mlnx_bridge_port_admin_state_get(_In_ const sai_object_key_t   *key,
                                                     _Inout_ sai_attribute_value_t *value,
                                                     _In_ uint32_t                  attr_index,
                                                     _Inout_ vendor_cache_t        *cache,
                                                     void                          *arg);
static sai_status_t mlnx_bridge_port_admin_state_set(_In_ const sai_object_key_t      *key,
                                                     _In_ const sai_attribute_value_t *value,
                                                     void                             *arg);
static sai_status_t mlnx_bridge_port_tagging_mode_get(_In_ const sai_object_key_t   *key,
                                                      _Inout_ sai_attribute_value_t *value,
                                                      _In_ uint32_t                  attr_index,
                                                      _Inout_ vendor_cache_t        *cache,
                                                      void                          *arg);
static sai_status_t mlnx_bridge_port_tagging_mode_set(_In_ const sai_object_key_t      *key,
                                                      _In_ const sai_attribute_value_t *value,
                                                      void                             *arg);
static sai_status_t mlnx_bridge_port_ingr_filter_get(_In_ const sai_object_key_t   *key,
                                                     _Inout_ sai_attribute_value_t *value,
                                                     _In_ uint32_t                  attr_index,
                                                     _Inout_ vendor_cache_t        *cache,
                                                     void                          *arg);
static sai_status_t mlnx_bridge_port_ingr_filter_set(_In_ const sai_object_key_t      *key,
                                                     _In_ const sai_attribute_value_t *value,
                                                     void                             *arg);
static sai_status_t mlnx_bridge_port_isolation_group_get(_In_ const sai_object_key_t   *key,
                                                         _Inout_ sai_attribute_value_t *value,
                                                         _In_ uint32_t                  attr_index,
                                                         _Inout_ vendor_cache_t        *cache,
                                                         void                          *arg);
static sai_status_t mlnx_bridge_port_isolation_group_set(_In_ const sai_object_key_t      *key,
                                                         _In_ const sai_attribute_value_t *value,
                                                         void                             *arg);
static sai_status_t mlnx_bridge_port_admin_state_set_internal(_In_ mlnx_bridge_port_t *bridge_port, _In_ bool is_up);
static sai_status_t mlnx_bridge_port_tagging_sai_to_sx(_In_ sai_bridge_port_tagging_mode_t sai_tagging_mode,
                                                       _Out_ sx_untagged_member_state_t   *sx_tagging_mode);
static const sai_vendor_attribute_entry_t bridge_port_vendor_attribs[] = {
    { SAI_BRIDGE_PORT_ATTR_TYPE,
      { true, false, false, true },
      { true, false, false, true },
      mlnx_bridge_port_type_get, NULL,
      NULL, NULL },
    { SAI_BRIDGE_PORT_ATTR_PORT_ID,
      { true, false, false, true },
      { true, false, false, true },
      mlnx_bridge_port_lag_or_port_get, NULL,
      NULL, NULL },
    { SAI_BRIDGE_PORT_ATTR_VLAN_ID,
      { true, false, false, true },
      { true, false, false, true },
      mlnx_bridge_port_vlan_id_get, NULL,
      NULL, NULL },
    { SAI_BRIDGE_PORT_ATTR_RIF_ID,
      { true, false, false, true },
      { true, false, false, true },
      mlnx_bridge_port_rif_id_get, NULL,
      NULL, NULL },
    { SAI_BRIDGE_PORT_ATTR_TUNNEL_ID,
      { true, false, false, true },
      { true, false, false, true },
      mlnx_bridge_port_tunnel_id_get, NULL,
      NULL, NULL },
    { SAI_BRIDGE_PORT_ATTR_BRIDGE_ID,
      { true, false, true, true },
      { true, false, true, true },
      mlnx_bridge_port_bridge_id_get, NULL,
      mlnx_bridge_port_bridge_id_set, NULL },
    { SAI_BRIDGE_PORT_ATTR_FDB_LEARNING_MODE,
      { true, false, true, true },
      { true, false, true, true },
      mlnx_bridge_port_fdb_learning_mode_get, NULL,
      mlnx_bridge_port_fdb_learning_mode_set, NULL },
    { SAI_BRIDGE_PORT_ATTR_MAX_LEARNED_ADDRESSES,
      { true, false, true, true },
      { true, false, true, true },
      mlnx_bridge_port_max_learned_addresses_get, NULL,
      mlnx_bridge_port_max_learned_addresses_set, NULL },
    { SAI_BRIDGE_PORT_ATTR_FDB_LEARNING_LIMIT_VIOLATION_PACKET_ACTION,
      { false, false, false, false },
      { true, false, true, true },
      NULL, NULL,
      NULL, NULL },
    { SAI_BRIDGE_PORT_ATTR_ADMIN_STATE,
      { true, false, true, true },
      { true, false, true, true },
      mlnx_bridge_port_admin_state_get, NULL,
      mlnx_bridge_port_admin_state_set, NULL },
    { SAI_BRIDGE_PORT_ATTR_TAGGING_MODE,
      { true, false, true, true },
      { true, false, true, true },
      mlnx_bridge_port_tagging_mode_get, NULL,
      mlnx_bridge_port_tagging_mode_set, NULL },
    { SAI_BRIDGE_PORT_ATTR_INGRESS_FILTERING,
      { true, false, true, true },
      { true, false, true, true },
      mlnx_bridge_port_ingr_filter_get, NULL,
      mlnx_bridge_port_ingr_filter_set, NULL },
    { SAI_BRIDGE_PORT_ATTR_ISOLATION_GROUP,
      { true, false, true, true },
      { true, false, true, true },
      mlnx_bridge_port_isolation_group_get, NULL,
      mlnx_bridge_port_isolation_group_set, NULL },
    { END_FUNCTIONALITY_ATTRIBS_ID,
      { false, false, false, false },
      { false, false, false, false },
      NULL, NULL,
      NULL, NULL }
};
static const mlnx_attr_enum_info_t        bridge_port_enum_info[] = {
    [SAI_BRIDGE_PORT_ATTR_TYPE] = ATTR_ENUM_VALUES_ALL(),
    [SAI_BRIDGE_PORT_ATTR_TAGGING_MODE] = ATTR_ENUM_VALUES_ALL(),
    [SAI_BRIDGE_PORT_ATTR_FDB_LEARNING_MODE] = ATTR_ENUM_VALUES_LIST(
        SAI_BRIDGE_PORT_FDB_LEARNING_MODE_DISABLE,
        SAI_BRIDGE_PORT_FDB_LEARNING_MODE_HW,
        SAI_BRIDGE_PORT_FDB_LEARNING_MODE_CPU_LOG)
};
static const sai_stat_capability_t        bridge_port_stats_capabilities[] = {
    { SAI_BRIDGE_PORT_STAT_IN_OCTETS, SAI_STATS_MODE_READ | SAI_STATS_MODE_READ_AND_CLEAR },
    { SAI_BRIDGE_PORT_STAT_OUT_OCTETS, SAI_STATS_MODE_READ | SAI_STATS_MODE_READ_AND_CLEAR },
    { SAI_BRIDGE_PORT_STAT_IN_PACKETS, SAI_STATS_MODE_READ | SAI_STATS_MODE_READ_AND_CLEAR },
    { SAI_BRIDGE_PORT_STAT_OUT_PACKETS, SAI_STATS_MODE_READ | SAI_STATS_MODE_READ_AND_CLEAR },
};
const mlnx_obj_type_attrs_info_t          mlnx_bridge_port_obj_type_info =
{ bridge_port_vendor_attribs, OBJ_ATTRS_ENUMS_INFO(bridge_port_enum_info), OBJ_STAT_CAP_INFO(
      bridge_port_stats_capabilities)};

sx_bridge_id_t mlnx_bridge_default_1q(void)
{
    return g_sai_db_ptr->sx_bridge_id;
}

sai_object_id_t mlnx_bridge_default_1q_oid(void)
{
    return g_sai_db_ptr->default_1q_bridge_oid;
}

static sai_object_id_t mlnx_bridge_dummy_1d_oid(void)
{
    return g_sai_db_ptr->dummy_1d_bridge_oid;
}

static sai_status_t mlnx_bridge_port_add(sx_bridge_id_t         bridge_id,
                                         sai_bridge_port_type_t port_type,
                                         mlnx_bridge_port_t   **port)
{
    mlnx_bridge_port_t *new_port;
    uint32_t            ii, db_start, db_end;

    if (port_type == SAI_BRIDGE_PORT_TYPE_PORT) {
        db_start = 0;
        db_end = MAX_BRIDGE_1Q_PORTS;
    } else {
        db_start = MAX_BRIDGE_1Q_PORTS;
        db_end = MAX_BRIDGE_PORTS;
    }

    for (ii = db_start; ii < db_end; ii++) {
        if (!g_sai_db_ptr->bridge_ports_db[ii].is_present) {
            new_port = &g_sai_db_ptr->bridge_ports_db[ii];

            new_port->bridge_id = bridge_id;
            new_port->port_type = port_type;
            new_port->is_present = true;
            new_port->index = ii;

            if (port_type != SAI_BRIDGE_PORT_TYPE_PORT) {
                g_sai_db_ptr->non_1q_bports_created++;
            }

            *port = new_port;
            return SAI_STATUS_SUCCESS;
        }
    }

    return SAI_STATUS_TABLE_FULL;
}

static sai_status_t mlnx_bridge_port_del(mlnx_bridge_port_t *port)
{
    if (port->index > MAX_BRIDGE_1Q_PORTS) {
        assert(g_sai_db_ptr->non_1q_bports_created > 0);
        g_sai_db_ptr->non_1q_bports_created--;
    }

    memset(port, 0, sizeof(*port));
    return SAI_STATUS_SUCCESS;
}

sai_status_t mlnx_bridge_port_by_idx(uint32_t idx, mlnx_bridge_port_t **port)
{
    if (idx >= MAX_BRIDGE_PORTS) {
        SX_LOG_ERR("Invalid bridge port idx - greater or equal than %u\n", MAX_BRIDGE_PORTS);
        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (!g_sai_db_ptr->bridge_ports_db[idx].is_present) {
        SX_LOG_ERR("Bridge port %d is removed or not created yet\n", idx);
        return SAI_STATUS_INVALID_PARAMETER;
    }

    *port = &g_sai_db_ptr->bridge_ports_db[idx];
    return SAI_STATUS_SUCCESS;
}

sai_status_t mlnx_bridge_port_by_oid(sai_object_id_t oid, mlnx_bridge_port_t **port)
{
    mlnx_object_id_t mlnx_bport_id = {0};
    sai_status_t     status;

    status = sai_to_mlnx_object_id(SAI_OBJECT_TYPE_BRIDGE_PORT, oid, &mlnx_bport_id);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed to convert bridge port oid %" PRIx64 " to mlnx object id\n", oid);
        return status;
    }

    return mlnx_bridge_port_by_idx(mlnx_bport_id.id.u32, port);
}

sai_status_t mlnx_bridge_port_by_tunnel_id(sx_tunnel_id_t sx_tunnel, mlnx_bridge_port_t **port)
{
    mlnx_bridge_port_t        *it;
    uint32_t                   ii, checked;
    const mlnx_tunnel_entry_t *tunnel_entry;

    mlnx_bridge_non1q_port_foreach(it, ii, checked) {
        if (it->port_type != SAI_BRIDGE_PORT_TYPE_TUNNEL) {
            continue;
        }

        tunnel_entry = &g_sai_tunnel_db_ptr->tunnel_entry_db[it->tunnel_idx];

        if ((tunnel_entry->sx_tunnel_id_ipv4 == sx_tunnel) && tunnel_entry->ipv4_created) {
            *port = it;
            return SAI_STATUS_SUCCESS;
        }

        if ((tunnel_entry->sx_tunnel_id_ipv6 == sx_tunnel) && tunnel_entry->ipv6_created) {
            *port = it;
            return SAI_STATUS_SUCCESS;
        }
    }

    return SAI_STATUS_ITEM_NOT_FOUND;
}

sai_status_t mlnx_bridge_port_by_log(sx_port_log_id_t log, mlnx_bridge_port_t **port)
{
    mlnx_bridge_port_t *it;
    uint32_t            ii, checked;

    mlnx_bridge_1q_port_foreach(it, ii) {
        if (it->logical == log) {
            *port = it;
            return SAI_STATUS_SUCCESS;
        }
    }

    mlnx_bridge_non1q_port_foreach(it, ii, checked) {
        if (it->logical == log) {
            *port = it;
            return SAI_STATUS_SUCCESS;
        }
    }

    return SAI_STATUS_INVALID_PORT_NUMBER;
}

sai_status_t mlnx_bridge_1q_port_by_log(sx_port_log_id_t log, mlnx_bridge_port_t **port)
{
    mlnx_bridge_port_t *it;
    uint32_t            ii;

    mlnx_bridge_1q_port_foreach(it, ii) {
        if (it->logical == log) {
            *port = it;
            return SAI_STATUS_SUCCESS;
        }
    }

    return SAI_STATUS_INVALID_PORT_NUMBER;
}

sai_status_t mlnx_bridge_port_to_oid(mlnx_bridge_port_t *port, sai_object_id_t *oid)
{
    mlnx_object_id_t mlnx_bport_id = {0};

    mlnx_bport_id.id.u32 = port->index;

    return mlnx_object_id_to_sai(SAI_OBJECT_TYPE_BRIDGE_PORT, &mlnx_bport_id, oid);
}

static bool mlnx_bridge_port_in_1q_by_log(sx_port_log_id_t log_id)
{
    mlnx_bridge_port_t *port;
    sai_status_t        status;

    status = mlnx_bridge_1q_port_by_log(log_id, &port);
    if (SAI_ERR(status)) {
        return false;
    }

    return port->bridge_id == mlnx_bridge_default_1q();
}

sai_status_t mlnx_bridge_rif_add(sx_router_id_t vrf_id, mlnx_bridge_rif_t **rif)
{
    mlnx_bridge_rif_t *new_rif;
    uint32_t           ii;

    for (ii = 0; ii < MAX_BRIDGE_RIFS; ii++) {
        if (!g_sai_db_ptr->bridge_rifs_db[ii].is_used) {
            new_rif = &g_sai_db_ptr->bridge_rifs_db[ii];

            new_rif->sx_data.vrf_id = vrf_id;
            new_rif->is_used = true;
            new_rif->index = ii;

            *rif = new_rif;
            return SAI_STATUS_SUCCESS;
        }
    }

    return SAI_STATUS_TABLE_FULL;
}

sai_status_t mlnx_bridge_rif_del(mlnx_bridge_rif_t *rif)
{
    memset(rif, 0, sizeof(*rif));
    return SAI_STATUS_SUCCESS;
}

sai_status_t mlnx_bridge_rif_by_idx(uint32_t idx, mlnx_bridge_rif_t **rif)
{
    if (idx >= MAX_BRIDGE_RIFS) {
        SX_LOG_ERR("Invalid bridge rif idx - greater or equal than %u\n", MAX_BRIDGE_RIFS);
        return SAI_STATUS_INVALID_PARAMETER;
    }

    *rif = &g_sai_db_ptr->bridge_rifs_db[idx];
    return SAI_STATUS_SUCCESS;
}

static sai_status_t check_attrs_port_type(_In_ const sai_object_key_t *key,
                                          _In_ uint32_t                count,
                                          _In_ const sai_attribute_t  *attrs)
{
    uint32_t ii;

    for (ii = 0; ii < count; ii++) {
        attr_port_type_check_t check = ATTR_PORT_IS_ENABLED | ATTR_PORT_IS_LAG_ENABLED;
        const sai_attribute_t *attr = &attrs[ii];
        sai_status_t           status;

        switch (attr->id) {
        case SAI_BRIDGE_PORT_ATTR_PORT_ID:
            status = check_port_type_attr(&attr->value.oid, 1, check, attr->id, ii);
            if (SAI_ERR(status)) {
                SX_LOG_ERR("Check port attr type failed port oid %" PRIx64 "\n", attr->value.oid);
                return status;
            }

            return SAI_STATUS_SUCCESS;
        }
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t mlnx_port_is_in_bridge_1q(const mlnx_port_config_t *port)
{
    mlnx_bridge_port_t *bridge_port;

    return mlnx_bridge_1q_port_by_log(port->logical, &bridge_port) == SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_create_bridge_object(_In_ sai_bridge_type_t       sai_br_type,
                                              _In_ mlnx_shm_rm_array_idx_t bridge_db_idx,
                                              _In_ sx_bridge_id_t          sx_bridge_id,
                                              _Out_ sai_object_id_t       *bridge_oid)
{
    mlnx_object_id_t mlnx_bridge_id;

    memset(&mlnx_bridge_id, 0, sizeof(mlnx_bridge_id));

    mlnx_bridge_id.field.sub_type = sai_br_type;
    mlnx_bridge_id.id.bridge_db_idx = bridge_db_idx;
    mlnx_bridge_id.ext.bridge.sx_bridge_id = sx_bridge_id;

    return mlnx_object_id_to_sai(SAI_OBJECT_TYPE_BRIDGE, &mlnx_bridge_id, bridge_oid);
}

sai_status_t mlnx_create_bridge_1d_object(sx_bridge_id_t sx_br_id, sai_object_id_t  *bridge_oid)
{
    sai_status_t            status;
    mlnx_shm_rm_array_idx_t idx = MLNX_SHM_RM_ARRAY_IDX_UNINITIALIZED;

    status = mlnx_bridge_sx_to_db_idx(sx_br_id, &idx);
    if (SAI_ERR(status)) {
        return status;
    }

    return mlnx_create_bridge_object(SAI_BRIDGE_TYPE_1D, idx, sx_br_id, bridge_oid);
}

sai_status_t mlnx_bridge_oid_to_id(sai_object_id_t oid, sx_bridge_id_t *bridge_id)
{
    mlnx_object_id_t mlnx_obj_id = {0};
    sai_status_t     status;

    status = sai_to_mlnx_object_id(SAI_OBJECT_TYPE_BRIDGE, oid, &mlnx_obj_id);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed to parse bridge oid\n");
        return status;
    }

    *bridge_id = mlnx_obj_id.ext.bridge.sx_bridge_id;

    return SAI_STATUS_SUCCESS;
}

sai_status_t mlnx_bridge_port_sai_to_log_port_not_locked(sai_object_id_t oid, sx_port_log_id_t *log_port)
{
    mlnx_bridge_port_t *port;
    sai_status_t        status;

    status = mlnx_bridge_port_by_oid(oid, &port);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed to lookup bridge port by oid %" PRIx64 "\n", oid);
        return status;
    }

    if ((port->port_type != SAI_BRIDGE_PORT_TYPE_PORT) && (port->port_type != SAI_BRIDGE_PORT_TYPE_SUB_PORT)) {
        SX_LOG_ERR("Invalid bridge port type %u - should be port or sub-port\n", port->port_type);
        status = SAI_STATUS_INVALID_PARAMETER;
        return status;
    }

    *log_port = port->logical;

    return status;
}

sai_status_t mlnx_bridge_port_sai_to_log_port(sai_object_id_t oid, sx_port_log_id_t *log_port)
{
    sai_status_t status;

    sai_db_read_lock();
    status = mlnx_bridge_port_sai_to_log_port_not_locked(oid, log_port);
    sai_db_unlock();

    return status;
}

sai_status_t mlnx_bridge_port_to_vlan_port(sai_object_id_t oid, sx_port_log_id_t *log_port)
{
    mlnx_bridge_port_t *port;
    sai_status_t        status;

    assert(log_port);

    sai_db_read_lock();

    status = mlnx_bridge_port_by_oid(oid, &port);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed to lookup bridge port by oid %" PRIx64 "\n", oid);
        goto out;
    }

    if (port->port_type != SAI_BRIDGE_PORT_TYPE_PORT) {
        SX_LOG_ERR("Invalid bridge port type %u - should be port\n", port->port_type);
        status = SAI_STATUS_INVALID_PARAMETER;
        goto out;
    }

    *log_port = port->logical;
out:
    sai_db_unlock();
    return status;
}

/* Used in case the log_port is a bridge port in .1Q bridge (actually regular log port) or vport */
sai_status_t mlnx_log_port_to_sai_bridge_port(sx_port_log_id_t log_port, sai_object_id_t *oid)
{
    sai_status_t status;

    status = mlnx_log_port_to_sai_bridge_port_soft(log_port, oid);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed lookup bridge port by logical id %x\n", log_port);
    }

    return status;
}

/* The same as mlnx_log_port_to_sai_bridge_port but without error message */
sai_status_t mlnx_log_port_to_sai_bridge_port_soft(sx_port_log_id_t log_port, sai_object_id_t *oid)
{
    sai_status_t        status;
    mlnx_bridge_port_t *port;

    sai_db_read_lock();

    status = mlnx_bridge_port_by_log(log_port, &port);
    if (SAI_ERR(status)) {
        goto out;
    }

    status = mlnx_bridge_port_to_oid(port, oid);

out:
    sai_db_unlock();
    return status;
}

static sai_status_t mlnx_bridge_sx_vport_set(_In_ sx_port_log_id_t           sx_port,
                                             _In_ sx_vlan_id_t               sx_vlan_id,
                                             _In_ bool                       is_create,
                                             _In_ sx_untagged_member_state_t sx_tagging_mode,
                                             _Out_ sx_port_log_id_t         *sx_vport)
{
    sx_status_t     sx_status;
    sx_vlan_ports_t vlan_port_list;

    assert(sx_vport);

    memset(&vlan_port_list, 0, sizeof(vlan_port_list));

    vlan_port_list.log_port = sx_port;
    vlan_port_list.is_untagged = sx_tagging_mode;

    /*
     * vport_set and vlan_ports_set are called in different order to prevent a packet getting to .1Q bridge.
     * It can happen when port is in the vlan but vport is not created/removed
     */
    if (is_create) {
        sx_status = sx_api_port_vport_set(gh_sdk, SX_ACCESS_CMD_ADD, sx_port, sx_vlan_id, sx_vport);
        if (SX_ERR(sx_status)) {
            SX_LOG_ERR("Failed to create vport {%x : %d} - %s\n", sx_port, sx_vlan_id, SX_STATUS_MSG(sx_status));
            return sdk_to_sai(sx_status);
        }

        sx_status = sx_api_vlan_ports_set(gh_sdk, SX_ACCESS_CMD_ADD, DEFAULT_ETH_SWID, sx_vlan_id, &vlan_port_list, 1);
        if (SX_ERR(sx_status)) {
            SX_LOG_ERR("Failed to add port %x to vlan %d - %s\n", sx_port, sx_vlan_id, SX_STATUS_MSG(sx_status));
            return sdk_to_sai(sx_status);
        }

        SX_LOG_DBG("Create vport {%x : %d}\n", sx_port, sx_vlan_id);
    } else {
        sx_status = sx_api_vlan_ports_set(gh_sdk,
                                          SX_ACCESS_CMD_DELETE,
                                          DEFAULT_ETH_SWID,
                                          sx_vlan_id,
                                          &vlan_port_list,
                                          1);
        if (SX_ERR(sx_status)) {
            SX_LOG_ERR("Failed to remove port %x from vlan %d - %s\n", sx_port, sx_vlan_id, SX_STATUS_MSG(sx_status));
            return sdk_to_sai(sx_status);
        }

        sx_status = sx_api_port_vport_set(gh_sdk, SX_ACCESS_CMD_DELETE, sx_port, sx_vlan_id, sx_vport);
        if (SX_ERR(sx_status)) {
            SX_LOG_ERR("Failed to delete vport {%x : %d} - %s\n", sx_port, sx_vlan_id, SX_STATUS_MSG(sx_status));
            return sdk_to_sai(sx_status);
        }

        SX_LOG_DBG("Removed vport {%x : %d}\n", sx_port, sx_vlan_id);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t mlnx_bridge_sx_vport_create(_In_ sx_port_log_id_t           sx_port,
                                         _In_ sx_vlan_id_t               sx_vlan_id,
                                         _In_ sx_untagged_member_state_t sx_tagging_mode,
                                         _Out_ sx_port_log_id_t         *sx_vport)
{
    return mlnx_bridge_sx_vport_set(sx_port, sx_vlan_id, true, sx_tagging_mode, sx_vport);
}

sai_status_t mlnx_bridge_sx_vport_delete(_In_ sx_port_log_id_t sx_port,
                                         _In_ sx_vlan_id_t     sx_vlan_id,
                                         _In_ sx_port_log_id_t sx_vport)
{
    return mlnx_bridge_sx_vport_set(sx_port, sx_vlan_id, false, SX_TAGGED_MEMBER, &sx_vport);
}

/**
 * @brief Bridge type
 *
 * @type sai_bridge_type_t
 * @flags MANDATORY_ON_CREATE | CREATE_ONLY
 */
static sai_status_t mlnx_bridge_type_get(_In_ const sai_object_key_t   *key,
                                         _Inout_ sai_attribute_value_t *value,
                                         _In_ uint32_t                  attr_index,
                                         _Inout_ vendor_cache_t        *cache,
                                         void                          *arg)
{
    mlnx_object_id_t mlnx_bridge_id = {0};
    sai_status_t     status;

    status = sai_to_mlnx_object_id(SAI_OBJECT_TYPE_BRIDGE, key->key.object_id, &mlnx_bridge_id);
    if (SAI_ERR(status)) {
        return status;
    }

    value->s32 = mlnx_bridge_id.field.sub_type;
    return status;
}

static sai_status_t mlnx_bridge_ports_get(_In_ sx_bridge_id_t        sx_bridge,
                                          _Out_ mlnx_bridge_port_t **bports,
                                          _Inout_ uint32_t          *bports_count)
{
    mlnx_bridge_port_t *bport;
    uint32_t            ii, checked = 0, bridge_ports_count = 0;

    assert(bports);
    assert(bports_count);

    mlnx_bridge_1q_port_foreach(bport, ii) {
        if (bport->bridge_id != sx_bridge) {
            continue;
        }

        if (bridge_ports_count >= *bports_count) {
            SX_LOG_ERR("Insufficient buffer size %u\n", *bports_count);
            return SAI_STATUS_BUFFER_OVERFLOW;
        }

        bports[bridge_ports_count] = bport;
        bridge_ports_count++;
    }

    mlnx_bridge_non1q_port_foreach(bport, ii, checked) {
        if (bport->bridge_id != sx_bridge) {
            continue;
        }

        if (bridge_ports_count >= *bports_count) {
            SX_LOG_ERR("Insufficient buffer size %u\n", *bports_count);
            return SAI_STATUS_BUFFER_OVERFLOW;
        }

        bports[bridge_ports_count] = bport;
        bridge_ports_count++;
    }

    *bports_count = bridge_ports_count;

    return SAI_STATUS_SUCCESS;
}

sai_status_t mlnx_bridge_sx_ports_get(_In_ sx_bridge_id_t     sx_bridge,
                                      _Out_ sx_port_log_id_t *sx_ports,
                                      _Inout_ uint32_t       *ports_count)
{
    sai_status_t        status;
    mlnx_bridge_port_t *bports[MAX_BRIDGE_1Q_PORTS] = {NULL};
    uint32_t            bports_count, ii;

    assert(sx_ports);
    assert(ports_count);

    bports_count = MAX_BRIDGE_1Q_PORTS;
    status = mlnx_bridge_ports_get(sx_bridge, bports, &bports_count);
    if (SAI_ERR(status)) {
        return status;
    }

    if (*ports_count < bports_count) {
        SX_LOG_ERR("Insufficient buffer size %u\n", *ports_count);
        return SAI_STATUS_BUFFER_OVERFLOW;
    }

    for (ii = 0; ii < bports_count; ii++) {
        sx_ports[ii] = bports[ii]->logical;
    }

    *ports_count = bports_count;

    return SAI_STATUS_SUCCESS;
}

/**
 * @brief List of bridge ports associated to this bridge
 *
 * @type sai_object_list_t
 * @objects SAI_OBJECT_TYPE_BRIDGE_PORT
 * @flags READ_ONLY
 */
static sai_status_t mlnx_bridge_port_list_get(_In_ const sai_object_key_t   *key,
                                              _Inout_ sai_attribute_value_t *value,
                                              _In_ uint32_t                  attr_index,
                                              _Inout_ vendor_cache_t        *cache,
                                              void                          *arg)
{
    mlnx_object_id_t    mlnx_bridge_id = {0};
    sx_bridge_id_t      bridge_id;
    sai_status_t        status;
    sai_object_id_t     bport_oids[MAX_BRIDGE_1Q_PORTS] = {0};
    mlnx_bridge_port_t *bports[MAX_BRIDGE_1Q_PORTS] = {NULL};
    uint32_t            bports_count, ii;

    SX_LOG_ENTER();

    status = sai_to_mlnx_object_id(SAI_OBJECT_TYPE_BRIDGE, key->key.object_id, &mlnx_bridge_id);
    if (SAI_ERR(status)) {
        return status;
    }
    bridge_id = mlnx_bridge_id.ext.bridge.sx_bridge_id;

    sai_db_read_lock();

    bports_count = MAX_BRIDGE_1Q_PORTS;
    status = mlnx_bridge_ports_get(bridge_id, bports, &bports_count);
    if (SAI_ERR(status)) {
        goto out;
    }

    for (ii = 0; ii < bports_count; ii++) {
        status = mlnx_bridge_port_to_oid(bports[ii], &bport_oids[ii]);
        if (SAI_ERR(status)) {
            SX_LOG_ERR("Failed to convert bridge port to oid\n");
            goto out;
        }
    }

    status = mlnx_fill_objlist(bport_oids, bports_count, &value->objlist);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed to fill bridge port list\n");
    }

out:
    sai_db_unlock();

    SX_LOG_EXIT();
    return status;
}

/**
 * @brief Maximum number of learned MAC addresses
 *
 * Zero means learning limit disable
 *
 * @type sai_uint32_t
 * @flags CREATE_AND_SET
 * @default 0
 */
static sai_status_t mlnx_bridge_max_learned_addresses_get(_In_ const sai_object_key_t   *key,
                                                          _Inout_ sai_attribute_value_t *value,
                                                          _In_ uint32_t                  attr_index,
                                                          _Inout_ vendor_cache_t        *cache,
                                                          void                          *arg)
{
    sai_status_t   status = SAI_STATUS_SUCCESS;
    sx_bridge_id_t sx_bridge_id;

    SX_LOG_ENTER();

    status = mlnx_bridge_oid_to_id(key->key.object_id, &sx_bridge_id);
    if (SAI_ERR(status)) {
        goto out;
    }

    if (sx_bridge_id == mlnx_bridge_default_1q()) {
        value->u32 = MLNX_FDB_LEARNING_NO_LIMIT_VALUE;
        goto out;
    }

    status = mlnx_vlan_bridge_max_learned_addresses_get(sx_bridge_id, &value->u32);
    if (SAI_ERR(status)) {
        goto out;
    }

out:
    SX_LOG_EXIT();
    return status;
}

/**
 * @brief Maximum number of learned MAC addresses
 *
 * Zero means learning limit disable
 *
 * @type sai_uint32_t
 * @flags CREATE_AND_SET
 * @default 0
 */
static sai_status_t mlnx_bridge_max_learned_addresses_set(_In_ const sai_object_key_t      *key,
                                                          _In_ const sai_attribute_value_t *value,
                                                          void                             *arg)
{
    sai_status_t   status = SAI_STATUS_SUCCESS;
    sx_bridge_id_t sx_bridge_id;
    uint32_t       limit;

    SX_LOG_ENTER();

    status = mlnx_bridge_oid_to_id(key->key.object_id, &sx_bridge_id);
    if (SAI_ERR(status)) {
        goto out;
    }

    limit = value->u32;

    if (sx_bridge_id == mlnx_bridge_default_1q()) {
        if (MLNX_FDB_IS_LEARNING_LIMIT_EXISTS(limit)) {
            SX_LOG_ERR("Unsupported value for the default .1Q Bridge. The only supported is %d (no limit)\n",
                       MLNX_FDB_LEARNING_NO_LIMIT_VALUE);
            status = SAI_STATUS_NOT_SUPPORTED;
        } else {
            status = SAI_STATUS_SUCCESS;
        }

        goto out;
    }

    status = mlnx_max_learned_addresses_value_validate(limit, 0);
    if (SAI_ERR(status)) {
        return status;
    }

    status = mlnx_vlan_bridge_max_learned_addresses_set(sx_bridge_id, limit);
    if (SAI_ERR(status)) {
        goto out;
    }

out:
    SX_LOG_EXIT();
    return status;
}

/**
 * @brief To disable learning on a bridge
 *
 * @type bool
 * @flags CREATE_AND_SET
 * @default false
 */
static sai_status_t mlnx_bridge_learn_disable_get(_In_ const sai_object_key_t   *key,
                                                  _Inout_ sai_attribute_value_t *value,
                                                  _In_ uint32_t                  attr_index,
                                                  _Inout_ vendor_cache_t        *cache,
                                                  void                          *arg)
{
    sx_bridge_id_t      sx_bridge_id;
    sx_status_t         status;
    sx_fdb_learn_mode_t mode;

    SX_LOG_ENTER();

    status = mlnx_bridge_oid_to_id(key->key.object_id, &sx_bridge_id);
    if (SAI_ERR(status)) {
        return status;
    }

    if (SX_STATUS_SUCCESS !=
        (status = sx_api_fdb_fid_learn_mode_get(gh_sdk, DEFAULT_ETH_SWID, sx_bridge_id, &mode))) {
        SX_LOG_ERR("Failed to get learn mode %s.\n", SX_STATUS_MSG(status));
        status = sdk_to_sai(status);
        goto out;
    }

    if (SX_FDB_LEARN_MODE_DONT_LEARN == mode) {
        value->booldata = true;
    } else {
        value->booldata = false;
    }

out:
    SX_LOG_EXIT();
    return status;
}

static sai_status_t mlnx_bridge_learn_disable_set_impl(_In_ sx_bridge_id_t sx_bridge_id, _In_ bool learn_disable)
{
    sx_status_t         status;
    sx_fdb_learn_mode_t mode;

    if (learn_disable) {
        mode = SX_FDB_LEARN_MODE_DONT_LEARN;
    } else {
        mode = SX_FDB_LEARN_MODE_AUTO_LEARN;
    }

    if (SX_STATUS_SUCCESS !=
        (status = sx_api_fdb_fid_learn_mode_set(gh_sdk, DEFAULT_ETH_SWID, sx_bridge_id, mode))) {
        SX_LOG_ERR("Failed to set learn mode %s.\n", SX_STATUS_MSG(status));
        status = sdk_to_sai(status);
        goto out;
    }

out:
    return status;
}

/**
 * @brief To disable learning on a bridge
 *
 * @type bool
 * @flags CREATE_AND_SET
 * @default false
 */
static sai_status_t mlnx_bridge_learn_disable_set(_In_ const sai_object_key_t      *key,
                                                  _In_ const sai_attribute_value_t *value,
                                                  void                             *arg)
{
    sx_bridge_id_t sx_bridge_id;
    sx_status_t    status;

    SX_LOG_ENTER();

    status = mlnx_bridge_oid_to_id(key->key.object_id, &sx_bridge_id);
    if (SAI_ERR(status)) {
        return status;
    }

    status = mlnx_bridge_learn_disable_set_impl(sx_bridge_id, value->booldata);

    SX_LOG_EXIT();
    return status;
}

static void bridge_key_to_str(_In_ sai_object_id_t bridge_id, _Out_ char *key_str)
{
    mlnx_object_id_t mlnx_bridge = {0};
    sai_status_t     status;
    const char      *br_type_name;

    status = sai_to_mlnx_object_id(SAI_OBJECT_TYPE_BRIDGE, bridge_id, &mlnx_bridge);
    if (SAI_ERR(status)) {
        snprintf(key_str, MAX_KEY_STR_LEN, "invalid bridge");
    } else {
        br_type_name = (mlnx_bridge.field.sub_type == SAI_BRIDGE_TYPE_1D) ? "1d" : "1q";

        snprintf(key_str, MAX_KEY_STR_LEN, "bridge %u (.%s)", mlnx_bridge.ext.bridge.sx_bridge_id, br_type_name);
    }
}

sai_status_t mlnx_bridge_availability_get(_In_ sai_object_id_t        switch_id,
                                          _In_ uint32_t               attr_count,
                                          _In_ const sai_attribute_t *attr_list,
                                          _Out_ uint64_t             *count)
{
    sx_status_t sx_status;
    uint32_t    bridge_existing = 0;

    assert(count);

    sx_status = sx_api_bridge_iter_get(gh_sdk, SX_ACCESS_CMD_GET, 0, NULL, NULL, &bridge_existing);
    if (SX_ERR(sx_status)) {
        SX_LOG_ERR("Failed to get count of 802.1D bridges - %s\n", SX_STATUS_MSG(sx_status));
        return sdk_to_sai(sx_status);
    }

    *count = (uint64_t)(MAX_BRIDGES_1D - bridge_existing);
    return SAI_STATUS_SUCCESS;
}

static mlnx_fid_flood_ctrl_type_t mlnx_bridge_flood_type_to_fid_type(_In_ sai_bridge_flood_control_type_t type)
{
    assert(type <= SAI_BRIDGE_FLOOD_CONTROL_TYPE_L2MC_GROUP);

    return (mlnx_fid_flood_ctrl_type_t)(type);
}

static mlnx_fid_flood_ctrl_attr_t mlnx_bridge_flood_ctrl_attr_to_fid_attr(_In_ sai_bridge_attr_t attr)
{
    switch (attr) {
    case SAI_BRIDGE_ATTR_UNKNOWN_UNICAST_FLOOD_CONTROL_TYPE:
        return MLNX_FID_FLOOD_CTRL_ATTR_UC;

    case SAI_BRIDGE_ATTR_UNKNOWN_MULTICAST_FLOOD_CONTROL_TYPE:
        return MLNX_FID_FLOOD_CTRL_ATTR_MC;

    case SAI_BRIDGE_ATTR_BROADCAST_FLOOD_CONTROL_TYPE:
        return MLNX_FID_FLOOD_CTRL_ATTR_BC;

    default:
        SX_LOG_ERR("Unexpected flood ctrl attr - %d\n", attr);
        assert(false);
        return MLNX_FID_FLOOD_CTRL_ATTR_MAX;
    }
}

static mlnx_fid_flood_ctrl_attr_t mlnx_bridge_flood_ctrl_group_attr_to_fid_attr(_In_ sai_bridge_attr_t attr)
{
    switch (attr) {
    case SAI_BRIDGE_ATTR_UNKNOWN_UNICAST_FLOOD_GROUP:
        return MLNX_FID_FLOOD_CTRL_ATTR_UC;

    case SAI_BRIDGE_ATTR_UNKNOWN_MULTICAST_FLOOD_GROUP:
        return MLNX_FID_FLOOD_CTRL_ATTR_MC;

    case SAI_BRIDGE_ATTR_BROADCAST_FLOOD_GROUP:
        return MLNX_FID_FLOOD_CTRL_ATTR_BC;

    default:
        SX_LOG_ERR("Unexpected flood ctrl group  attr - %d\n", attr);
        assert(false);
        return MLNX_FID_FLOOD_CTRL_ATTR_MAX;
    }
}

static sai_bridge_flood_control_type_t mlnx_fid_flood_type_to_bridge_type(_In_ mlnx_fid_flood_ctrl_type_t type)
{
    assert(type <= MLNX_FID_FLOOD_TYPE_L2MC_GROUP);

    return (sai_bridge_flood_control_type_t)(type);
}

/**
 * Unknown unicast/multicast, broadcast flood control type
 */
static sai_status_t mlnx_bridge_flood_ctrl_get(_In_ const sai_object_key_t   *key,
                                               _Inout_ sai_attribute_value_t *value,
                                               _In_ uint32_t                  attr_index,
                                               _Inout_ vendor_cache_t        *cache,
                                               void                          *arg)
{
    sx_status_t                       status;
    sai_bridge_attr_t                 attr;
    mlnx_bridge_t                    *bridge;
    mlnx_fid_flood_ctrl_attr_t        flood_ctrl_attr;
    const mlnx_fid_flood_type_data_t *flood_ctrl_data;

    SX_LOG_ENTER();

    attr = (long)(arg);

    assert((attr == SAI_BRIDGE_ATTR_UNKNOWN_UNICAST_FLOOD_CONTROL_TYPE) ||
           (attr == SAI_BRIDGE_ATTR_UNKNOWN_MULTICAST_FLOOD_CONTROL_TYPE) ||
           (attr == SAI_BRIDGE_ATTR_BROADCAST_FLOOD_CONTROL_TYPE));

    sai_db_read_lock();

    status = mlnx_bridge_1d_oid_to_data(key->key.object_id, &bridge, NULL);
    if (SAI_ERR(status)) {
        sai_db_unlock();
        SX_LOG_EXIT();
        return status;
    }

    flood_ctrl_attr = mlnx_bridge_flood_ctrl_attr_to_fid_attr(attr);
    flood_ctrl_data = &bridge->flood_data.types[flood_ctrl_attr];

    value->s32 = mlnx_fid_flood_type_to_bridge_type(flood_ctrl_data->type);

    sai_db_unlock();
    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

/**
 * Unknown unicast/multicast, broadcast flood control type
 */
static sai_status_t mlnx_bridge_flood_ctrl_set(_In_ const sai_object_key_t      *key,
                                               _In_ const sai_attribute_value_t *value,
                                               void                             *arg)
{
    sai_status_t                status;
    sai_bridge_attr_t           attr;
    mlnx_bridge_t              *bridge;
    mlnx_fid_flood_ctrl_attr_t  flood_ctrl_attr;
    mlnx_fid_flood_type_data_t *flood_ctrl_data;
    mlnx_fid_flood_ctrl_type_t  new_type;

    SX_LOG_ENTER();

    attr = (long)(arg);

    assert((attr == SAI_BRIDGE_ATTR_UNKNOWN_UNICAST_FLOOD_CONTROL_TYPE) ||
           (attr == SAI_BRIDGE_ATTR_UNKNOWN_MULTICAST_FLOOD_CONTROL_TYPE) ||
           (attr == SAI_BRIDGE_ATTR_BROADCAST_FLOOD_CONTROL_TYPE));

    new_type = mlnx_bridge_flood_type_to_fid_type(value->s32);

    sai_db_write_lock();

    status = mlnx_bridge_1d_oid_to_data(key->key.object_id, &bridge, NULL);
    if (SAI_ERR(status)) {
        goto out;
    }

    flood_ctrl_attr = mlnx_bridge_flood_ctrl_attr_to_fid_attr(attr);
    flood_ctrl_data = &bridge->flood_data.types[flood_ctrl_attr];

    status = mlnx_fid_flood_ctrl_type_set(bridge->sx_bridge_id, flood_ctrl_attr, flood_ctrl_data, new_type);
    if (SAI_ERR(status)) {
        goto out;
    }

out:
    sai_db_unlock();
    SX_LOG_EXIT();
    return status;
}

/**
 * Unknown unicast/multicast, broadcast flood group.
 */
static sai_status_t mlnx_bridge_flood_group_get(_In_ const sai_object_key_t   *key,
                                                _Inout_ sai_attribute_value_t *value,
                                                _In_ uint32_t                  attr_index,
                                                _Inout_ vendor_cache_t        *cache,
                                                void                          *arg)
{
    sai_status_t                      status;
    sai_bridge_attr_t                 attr;
    mlnx_bridge_t                    *bridge;
    const mlnx_fid_flood_type_data_t *flood_ctrl_data;
    mlnx_fid_flood_ctrl_attr_t        flood_ctrl_attr;

    SX_LOG_ENTER();

    attr = (long)(arg);

    assert((attr == SAI_BRIDGE_ATTR_UNKNOWN_UNICAST_FLOOD_GROUP) ||
           (attr == SAI_BRIDGE_ATTR_UNKNOWN_MULTICAST_FLOOD_GROUP) ||
           (attr == SAI_BRIDGE_ATTR_BROADCAST_FLOOD_GROUP));

    sai_db_read_lock();

    status = mlnx_bridge_1d_oid_to_data(key->key.object_id, &bridge, NULL);
    if (SAI_ERR(status)) {
        goto out;
    }

    flood_ctrl_attr = mlnx_bridge_flood_ctrl_group_attr_to_fid_attr(attr);
    flood_ctrl_data = &bridge->flood_data.types[flood_ctrl_attr];

    value->oid = SAI_NULL_OBJECT_ID;

    if (MLNX_L2MC_GROUP_DB_IDX_IS_VALID(flood_ctrl_data->l2mc_db_idx)) {
        status = mlnx_l2mc_group_oid_create(&l2mc_group_db(flood_ctrl_data->l2mc_db_idx), &value->oid);
        if (SAI_ERR(status)) {
            goto out;
        }
    }

out:
    sai_db_unlock();
    SX_LOG_EXIT();
    return status;
}

/**
 * Unknown unicast/multicast, broadcast flood group.
 */
static sai_status_t mlnx_bridge_flood_group_set(_In_ const sai_object_key_t      *key,
                                                _In_ const sai_attribute_value_t *value,
                                                void                             *arg)
{
    sai_status_t                status;
    sai_bridge_attr_t           attr;
    mlnx_bridge_t              *bridge;
    mlnx_fid_flood_type_data_t *flood_ctrl_data;
    mlnx_fid_flood_ctrl_attr_t  flood_ctrl_attr;

    SX_LOG_ENTER();

    attr = (long)(arg);

    assert((attr == SAI_BRIDGE_ATTR_UNKNOWN_UNICAST_FLOOD_GROUP) ||
           (attr == SAI_BRIDGE_ATTR_UNKNOWN_MULTICAST_FLOOD_GROUP) ||
           (attr == SAI_BRIDGE_ATTR_BROADCAST_FLOOD_GROUP));

    sai_db_write_lock();

    status = mlnx_bridge_1d_oid_to_data(key->key.object_id, &bridge, NULL);
    if (SAI_ERR(status)) {
        goto out;
    }

    flood_ctrl_attr = mlnx_bridge_flood_ctrl_group_attr_to_fid_attr(attr);
    flood_ctrl_data = &bridge->flood_data.types[flood_ctrl_attr];

    status = mlnx_fid_flood_ctrl_l2mc_group_set(bridge->sx_bridge_id, flood_ctrl_attr, flood_ctrl_data, value->oid);
    if (SAI_ERR(status)) {
        goto out;
    }

out:
    sai_db_unlock();
    SX_LOG_EXIT();
    return status;
}

static sai_status_t mlnx_bridge_sx_to_db_idx(_In_ sx_bridge_id_t sx_bridge_id, _Out_ mlnx_shm_rm_array_idx_t *idx)
{
    if (sx_bridge_id < MIN_SX_BRIDGE_ID) {
        SX_LOG_ERR("sx_bridge_id (%d) >= MIN_SX_BRIDGE_ID (%d)\n", sx_bridge_id, MIN_SX_BRIDGE_ID);
        return SAI_STATUS_FAILURE;
    }

    idx->type = MLNX_SHM_RM_ARRAY_TYPE_BRIDGE;
    idx->idx = sx_bridge_id - MIN_SX_BRIDGE_ID;

    return SAI_STATUS_SUCCESS;
}

static mlnx_bridge_t* mlnx_bridge_1d_by_rm_idx(_In_ mlnx_shm_rm_array_idx_t rm_idx)
{
    sai_status_t status;
    void        *ptr;

    status = mlnx_shm_rm_array_idx_to_ptr(rm_idx, &ptr);
    if (SAI_ERR(status)) {
        return NULL;
    }

    return (mlnx_bridge_t*)ptr;
}

mlnx_bridge_t* mlnx_bridge_1d_by_db_idx(_In_ uint32_t db_idx)
{
    mlnx_shm_rm_array_idx_t rm_idx;

    rm_idx.type = MLNX_SHM_RM_ARRAY_TYPE_BRIDGE;
    rm_idx.idx = db_idx;

    return mlnx_bridge_1d_by_rm_idx(rm_idx);
}

static sai_status_t mlnx_bridge_1d_oid_to_data(_In_ sai_object_id_t           bridge_oid,
                                               _Out_ mlnx_bridge_t          **bridge,
                                               _Out_ mlnx_shm_rm_array_idx_t *idx)
{
    sai_status_t            status;
    mlnx_object_id_t        mlnx_bridge_id;
    mlnx_shm_rm_array_idx_t db_idx;

    assert(bridge);

    status = sai_to_mlnx_object_id(SAI_OBJECT_TYPE_BRIDGE, bridge_oid, &mlnx_bridge_id);
    if (SAI_ERR(status)) {
        return status;
    }

    if (mlnx_bridge_id.field.sub_type != SAI_BRIDGE_TYPE_1D) {
        SX_LOG_ERR("Unexpected bridge type %d is not 1D", mlnx_bridge_id.field.sub_type);
        return SAI_STATUS_FAILURE;
    }

    db_idx = mlnx_bridge_id.id.bridge_db_idx;

    *bridge = mlnx_bridge_1d_by_rm_idx(db_idx);

    if (!(*bridge)->array_hdr.is_used) {
        SX_LOG_ERR("Bridge db idx %d is removed or not created yet\n", db_idx.idx);
        return SAI_STATUS_FAILURE;
    }

    if (idx) {
        *idx = db_idx;
    }

    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_bridge_db_alloc(_In_ sx_bridge_id_t            sx_bridge_id,
                                         _Out_ mlnx_bridge_t          **bridge,
                                         _Out_ mlnx_shm_rm_array_idx_t *idx)
{
    sai_status_t status;

    assert(bridge);
    assert(idx);

    status = mlnx_bridge_sx_to_db_idx(sx_bridge_id, idx);
    if (SAI_ERR(status)) {
        return SAI_STATUS_FAILURE;
    }

    *bridge = mlnx_bridge_1d_by_rm_idx(*idx);

    (*bridge)->array_hdr.is_used = true;

    memset(&(*bridge)->flood_data, 0, sizeof(mlnx_fid_flood_data_t));

    SX_LOG_DBG("Allocated db idx %d for sx bridge %d\n", idx->idx, sx_bridge_id);

    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_bridge_db_free(_In_ mlnx_shm_rm_array_idx_t idx, _In_ mlnx_bridge_t           *bridge)
{
    assert(bridge->array_hdr.is_used);

    memset(&bridge->flood_data, 0, sizeof(mlnx_fid_flood_data_t));

    bridge->sx_bridge_id = 0;
    bridge->array_hdr.is_used = false;

    return SAI_STATUS_SUCCESS;
}

/**
 * @brief Create bridge
 *
 * @param[out] bridge_id Bridge ID
 * @param[in] switch_id Switch object id
 * @param[in] attr_count number of attributes
 * @param[in] attr_list array of attributes
 *
 * @return #SAI_STATUS_SUCCESS on success Failure status code on error
 */
static sai_status_t mlnx_create_bridge(_Out_ sai_object_id_t     * bridge_id,
                                       _In_ sai_object_id_t        switch_id,
                                       _In_ uint32_t               attr_count,
                                       _In_ const sai_attribute_t *attr_list)
{
    char                         list_str[MAX_LIST_VALUE_STR_LEN];
    char                         key_str[MAX_KEY_STR_LEN];
    mlnx_bridge_t               *bridge;
    sx_bridge_id_t               sx_bridge_id;
    const sai_attribute_value_t *attr_val, *max_learned_addresses = NULL;
    uint32_t                     attr_idx, max_learned_addresses_index;
    mlnx_shm_rm_array_idx_t      bridge_db_idx;
    sai_status_t                 status;

    SX_LOG_ENTER();

    if (NULL == bridge_id) {
        SX_LOG_ERR("NULL bridge ID param\n");
        return SAI_STATUS_INVALID_PARAMETER;
    }

    status = check_attribs_metadata(attr_count, attr_list, SAI_OBJECT_TYPE_BRIDGE, bridge_vendor_attribs,
                                    SAI_COMMON_API_CREATE);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed attribs check\n");
        return status;
    }

    sai_attr_list_to_str(attr_count, attr_list, SAI_OBJECT_TYPE_BRIDGE, MAX_LIST_VALUE_STR_LEN, list_str);
    SX_LOG_NTC("Create bridge, %s\n", list_str);

    status = find_attrib_in_list(attr_count, attr_list, SAI_BRIDGE_ATTR_TYPE, &attr_val, &attr_idx);
    assert(!SAI_ERR(status));

    if (attr_val->s32 != SAI_BRIDGE_TYPE_1D) {
        SX_LOG_ERR("Not supported bridge type %d\n", attr_val->s32);
        return SAI_STATUS_INVALID_PARAMETER;
    }

    status = find_attrib_in_list(attr_count, attr_list, SAI_BRIDGE_ATTR_MAX_LEARNED_ADDRESSES,
                                 &max_learned_addresses, &max_learned_addresses_index);
    if (!SAI_ERR(status)) {
        status = mlnx_max_learned_addresses_value_validate(max_learned_addresses->u32, max_learned_addresses_index);
        if (SAI_ERR(status)) {
            return status;
        }
    }

    sai_db_write_lock();

    status = mlnx_sdk_bridge_create(&sx_bridge_id);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed to create .1D bridge\n");
        goto out;
    }

    status = mlnx_bridge_db_alloc(sx_bridge_id, &bridge, &bridge_db_idx);
    if (SAI_ERR(status)) {
        goto out;
    }

    mlnx_fid_flood_ctrl_init(&bridge->flood_data);

    bridge->sx_bridge_id = sx_bridge_id;

    if (max_learned_addresses) {
        status = mlnx_vlan_bridge_max_learned_addresses_set(bridge->sx_bridge_id, max_learned_addresses->u32);
        if (SAI_ERR(status)) {
            goto out;
        }
    }

    find_attrib_in_list(attr_count, attr_list, SAI_BRIDGE_ATTR_UNKNOWN_UNICAST_FLOOD_CONTROL_TYPE, &attr_val,
                        &attr_idx);
    if (attr_val) {
        bridge->flood_data.types[MLNX_FID_FLOOD_CTRL_ATTR_UC].type = mlnx_bridge_flood_type_to_fid_type(attr_val->s32);
    }

    find_attrib_in_list(attr_count, attr_list, SAI_BRIDGE_ATTR_UNKNOWN_UNICAST_FLOOD_GROUP, &attr_val, &attr_idx);
    if (attr_val) {
        status = mlnx_l2mc_group_oid_to_db_idx(attr_val->oid,
                                               &bridge->flood_data.types[MLNX_FID_FLOOD_CTRL_ATTR_UC].l2mc_db_idx);
        if (SAI_ERR(status)) {
            goto out;
        }
    }

    find_attrib_in_list(attr_count,
                        attr_list,
                        SAI_BRIDGE_ATTR_UNKNOWN_MULTICAST_FLOOD_CONTROL_TYPE,
                        &attr_val,
                        &attr_idx);
    if (attr_val) {
        bridge->flood_data.types[MLNX_FID_FLOOD_CTRL_ATTR_MC].type = mlnx_bridge_flood_type_to_fid_type(attr_val->s32);
    }

    find_attrib_in_list(attr_count, attr_list, SAI_BRIDGE_ATTR_UNKNOWN_MULTICAST_FLOOD_GROUP, &attr_val, &attr_idx);
    if (attr_val) {
        status = mlnx_l2mc_group_oid_to_db_idx(attr_val->oid,
                                               &bridge->flood_data.types[MLNX_FID_FLOOD_CTRL_ATTR_MC].l2mc_db_idx);
        if (SAI_ERR(status)) {
            goto out;
        }
    }

    find_attrib_in_list(attr_count, attr_list, SAI_BRIDGE_ATTR_BROADCAST_FLOOD_CONTROL_TYPE, &attr_val, &attr_idx);
    if (attr_val) {
        bridge->flood_data.types[MLNX_FID_FLOOD_CTRL_ATTR_BC].type = mlnx_bridge_flood_type_to_fid_type(attr_val->s32);
    }

    find_attrib_in_list(attr_count, attr_list, SAI_BRIDGE_ATTR_BROADCAST_FLOOD_GROUP, &attr_val, &attr_idx);
    if (attr_val) {
        status = mlnx_l2mc_group_oid_to_db_idx(attr_val->oid,
                                               &bridge->flood_data.types[MLNX_FID_FLOOD_CTRL_ATTR_BC].l2mc_db_idx);
        if (SAI_ERR(status)) {
            goto out;
        }
    }

    mlnx_fid_flood_ctrl_l2mc_group_refs_inc(&bridge->flood_data);

    status = find_attrib_in_list(attr_count, attr_list, SAI_BRIDGE_ATTR_LEARN_DISABLE, &attr_val, &attr_idx);
    if (!SAI_ERR(status)) {
        status = mlnx_bridge_learn_disable_set_impl(sx_bridge_id, attr_val->booldata);
        if (SAI_ERR(status)) {
            goto out;
        }
    }

    status = mlnx_create_bridge_object(SAI_BRIDGE_TYPE_1D, bridge_db_idx, bridge->sx_bridge_id, bridge_id);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed to create bridge oid\n");
        goto out;
    }

    bridge_key_to_str(*bridge_id, key_str);
    SX_LOG_NTC("Created %s\n", key_str);

out:
    sai_db_unlock();
    SX_LOG_EXIT();
    return status;
}

static sai_status_t mlnx_bridge_is_in_use(_In_ const mlnx_bridge_t *bridge, _Out_ bool               *is_in_use)
{
    mlnx_bridge_port_t *port;
    uint32_t            ii, checked;
    bool                has_ports = false;

    assert(bridge);
    assert(is_in_use);

    *is_in_use = false;

    mlnx_bridge_1q_port_foreach(port, ii) {
        if (port->bridge_id == bridge->sx_bridge_id) {
            has_ports = true;
            break;
        }
    }

    if (!has_ports) {
        mlnx_bridge_non1q_port_foreach(port, ii, checked) {
            if (port->bridge_id == bridge->sx_bridge_id) {
                has_ports = true;
                break;
            }
        }
    }

    if (has_ports) {
        SX_LOG_ERR("Bridge sx %x has ports\n", bridge->sx_bridge_id);
        *is_in_use = true;
    }

    return SAI_STATUS_SUCCESS;
}

/**
 * @brief Remove bridge
 *
 * @param[in] bridge_id Bridge ID
 *
 * @return #SAI_STATUS_SUCCESS on success Failure status code on error
 */
static sai_status_t mlnx_remove_bridge(_In_ sai_object_id_t bridge_id)
{
    sx_status_t             sx_status;
    sai_status_t            status;
    mlnx_bridge_t          *bridge;
    mlnx_shm_rm_array_idx_t idx;
    bool                    is_in_use;

    SX_LOG_ENTER();

    sai_db_read_lock();

    if (bridge_id == mlnx_bridge_default_1q_oid()) {
        SX_LOG_ERR("Could not remove default .1Q bridge\n");
        status = SAI_STATUS_INVALID_PARAMETER;
        goto out;
    }

    if (bridge_id == mlnx_bridge_dummy_1d_oid()) {
        SX_LOG_ERR("Could not remove dummy .1D bridge\n");
        status = SAI_STATUS_INVALID_PARAMETER;
        goto out;
    }

    status = mlnx_bridge_1d_oid_to_data(bridge_id, &bridge, &idx);
    if (SAI_ERR(status)) {
        goto out;
    }

    status = mlnx_bridge_is_in_use(bridge, &is_in_use);
    if (SAI_ERR(status)) {
        goto out;
    }

    if (is_in_use) {
        SX_LOG_ERR("Failed to remove bridge %lx - in use\n", bridge_id);
        status = SAI_STATUS_OBJECT_IN_USE;
        goto out;
    }

    status = mlnx_fid_flood_ctrl_clear(bridge->sx_bridge_id);
    if (SAI_ERR(status)) {
        goto out;
    }

    sx_status = sx_api_bridge_set(gh_sdk, SX_ACCESS_CMD_DESTROY, &bridge->sx_bridge_id);
    if (SX_ERR(sx_status)) {
        SX_LOG_ERR("Failed to remove .1D bridge - %s\n", SX_STATUS_MSG(sx_status));
        status = sdk_to_sai(sx_status);
        goto out;
    }

    mlnx_fid_flood_ctrl_l2mc_group_refs_dec(&bridge->flood_data);

    status = mlnx_bridge_db_free(idx, bridge);
    if (SAI_ERR(status)) {
        goto out;
    }

    SX_LOG_NTC("Removed bridge id %" PRIx64 "\n", bridge_id);

out:
    sai_db_unlock();

    SX_LOG_EXIT();
    return status;
}

/**
 * @brief Set attribute for bridge
 *
 * @param[in] bridge_id Bridge ID
 * @param[in] attr attribute to set
 *
 * @return #SAI_STATUS_SUCCESS on success Failure status code on error
 */
static sai_status_t mlnx_set_bridge_attribute(_In_ sai_object_id_t bridge_id, _In_ const sai_attribute_t *attr)
{
    const sai_object_key_t key = { .key.object_id = bridge_id };
    char                   key_str[MAX_KEY_STR_LEN];

    SX_LOG_ENTER();

    bridge_key_to_str(bridge_id, key_str);
    return sai_set_attribute(&key, key_str, SAI_OBJECT_TYPE_BRIDGE, bridge_vendor_attribs, attr);
}

/**
 * @brief Get attributes of bridge
 *
 * @param[in] bridge_id Bridge ID
 * @param[in] attr_count number of attributes
 * @param[inout] attr_list array of attributes
 *
 * @return #SAI_STATUS_SUCCESS on success Failure status code on error
 */
static sai_status_t mlnx_get_bridge_attribute(_In_ sai_object_id_t     bridge_id,
                                              _In_ uint32_t            attr_count,
                                              _Inout_ sai_attribute_t *attr_list)
{
    const sai_object_key_t key = { .key.object_id = bridge_id };
    char                   key_str[MAX_KEY_STR_LEN];

    SX_LOG_ENTER();

    bridge_key_to_str(bridge_id, key_str);
    return sai_get_attributes(&key,
                              key_str,
                              SAI_OBJECT_TYPE_BRIDGE,
                              bridge_vendor_attribs,
                              attr_count,
                              attr_list);
}

/**
 * @brief Bridge port type
 *
 * @type sai_bridge_port_type_t
 * @flags MANDATORY_ON_CREATE | CREATE_ONLY
 */
static sai_status_t mlnx_bridge_port_type_get(_In_ const sai_object_key_t   *key,
                                              _Inout_ sai_attribute_value_t *value,
                                              _In_ uint32_t                  attr_index,
                                              _Inout_ vendor_cache_t        *cache,
                                              void                          *arg)
{
    mlnx_bridge_port_t *port;
    sai_status_t        status;

    SX_LOG_ENTER();

    sai_db_read_lock();

    status = mlnx_bridge_port_by_oid(key->key.object_id, &port);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed to lookup bridge port by oid %" PRIx64 "\n", key->key.object_id);
        goto out;
    }

    value->s32 = port->port_type;

out:
    sai_db_unlock();
    SX_LOG_EXIT();
    return status;
}

/**
 * @brief Associated Port or Lag object id
 *
 * @type sai_object_id_t
 * @objects SAI_OBJECT_TYPE_PORT, SAI_OBJECT_TYPE_LAG
 * @flags MANDATORY_ON_CREATE | CREATE_ONLY
 * @condition SAI_BRIDGE_PORT_ATTR_TYPE == SAI_BRIDGE_PORT_TYPE_PORT or SAI_BRIDGE_PORT_ATTR_TYPE == SAI_BRIDGE_PORT_TYPE_SUB_PORT
 */
static sai_status_t mlnx_bridge_port_lag_or_port_get(_In_ const sai_object_key_t   *key,
                                                     _Inout_ sai_attribute_value_t *value,
                                                     _In_ uint32_t                  attr_index,
                                                     _Inout_ vendor_cache_t        *cache,
                                                     void                          *arg)
{
    sx_port_log_id_t    log_port;
    sai_status_t        status;
    mlnx_bridge_port_t *port;

    SX_LOG_ENTER();

    sai_db_read_lock();

    status = mlnx_bridge_port_by_oid(key->key.object_id, &port);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed to lookup bridge port by oid %" PRIx64 " \n", key->key.object_id);
        goto out;
    }

    if (port->port_type == SAI_BRIDGE_PORT_TYPE_PORT) {
        log_port = port->logical;
    } else if (port->port_type == SAI_BRIDGE_PORT_TYPE_SUB_PORT) {
        log_port = port->parent;
    } else {
        SX_LOG_ERR("Invalid port type - %d\n", port->port_type);
        status = SAI_STATUS_INVALID_PARAMETER;
        goto out;
    }

    status = mlnx_log_port_to_object(log_port, &value->oid);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed to convert log port %x to port oid\n", port->logical);
    }

out:
    sai_db_unlock();
    SX_LOG_EXIT();
    return status;
}

/**
 * @brief Associated Vlan
 *
 * @type sai_uint16_t
 * @flags MANDATORY_ON_CREATE | CREATE_ONLY
 * @condition SAI_BRIDGE_PORT_ATTR_TYPE == SAI_BRIDGE_PORT_TYPE_SUB_PORT
 * @isvlan true
 */
static sai_status_t mlnx_bridge_port_vlan_id_get(_In_ const sai_object_key_t   *key,
                                                 _Inout_ sai_attribute_value_t *value,
                                                 _In_ uint32_t                  attr_index,
                                                 _Inout_ vendor_cache_t        *cache,
                                                 void                          *arg)
{
    mlnx_bridge_port_t *port;
    sai_status_t        status;

    SX_LOG_ENTER();

    sai_db_read_lock();

    status = mlnx_bridge_port_by_oid(key->key.object_id, &port);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed to lookup bridge port by oid %" PRIx64 "\n", key->key.object_id);
        goto out;
    }

    if (port->port_type != SAI_BRIDGE_PORT_TYPE_SUB_PORT) {
        SX_LOG_ERR("Invalid bridge port type %d, must be SAI_BRIDGE_PORT_TYPE_SUB_PORT\n", port->port_type);
        status = SAI_STATUS_INVALID_PARAMETER;
        goto out;
    }

    value->u16 = port->vlan_id;

out:
    sai_db_unlock();
    SX_LOG_EXIT();
    return status;
}

/**
 * @brief Associated router interface object id
 * Please note that for SAI_BRIDGE_PORT_TYPE_1Q_ROUTER,
 * all vlan interfaces are auto bounded for the bridge port.
 *
 * @type sai_object_id_t
 * @objects SAI_OBJECT_TYPE_ROUTER_INTERFACE
 * @flags MANDATORY_ON_CREATE | CREATE_ONLY
 * @condition SAI_BRIDGE_PORT_ATTR_TYPE == SAI_BRIDGE_PORT_TYPE_1D_ROUTER
 */
static sai_status_t mlnx_bridge_port_rif_id_get(_In_ const sai_object_key_t   *key,
                                                _Inout_ sai_attribute_value_t *value,
                                                _In_ uint32_t                  attr_index,
                                                _Inout_ vendor_cache_t        *cache,
                                                void                          *arg)
{
    mlnx_bridge_rif_t  *bridge_rif;
    mlnx_bridge_port_t *port;
    sai_status_t        status;

    SX_LOG_ENTER();

    sai_db_read_lock();

    status = mlnx_bridge_port_by_oid(key->key.object_id, &port);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed to lookup bridge port by oid %" PRIx64 "\n", key->key.object_id);
        goto out;
    }

    if (port->port_type != SAI_BRIDGE_PORT_TYPE_1D_ROUTER) {
        SX_LOG_ERR("Invalid bridge port type %d, SAI_BRIDGE_PORT_TYPE_1D_ROUTER is only supported\n",
                   port->port_type);

        status = SAI_STATUS_INVALID_PARAMETER;
        goto out;
    }

    status = mlnx_bridge_rif_by_idx(port->rif_index, &bridge_rif);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed to lookup bridge rif by index %u\n", port->rif_index);
        goto out;
    }

    status = mlnx_rif_oid_create(MLNX_RIF_TYPE_BRIDGE, bridge_rif, MLNX_SHM_RM_ARRAY_IDX_UNINITIALIZED, &value->oid);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed to convert rif to oid\n");
        goto out;
    }

out:
    sai_db_unlock();
    SX_LOG_EXIT();
    return status;
}

/**
 * @brief Associated tunnel id
 *
 * @type sai_object_id_t
 * @objects SAI_OBJECT_TYPE_TUNNEL
 * @flags MANDATORY_ON_CREATE | CREATE_ONLY
 * @condition SAI_BRIDGE_PORT_ATTR_TYPE == SAI_BRIDGE_PORT_TYPE_TUNNEL
 */
static sai_status_t mlnx_bridge_port_tunnel_id_get(_In_ const sai_object_key_t   *key,
                                                   _Inout_ sai_attribute_value_t *value,
                                                   _In_ uint32_t                  attr_index,
                                                   _Inout_ vendor_cache_t        *cache,
                                                   void                          *arg)
{
    mlnx_bridge_port_t *port;
    sai_status_t        status;

    SX_LOG_ENTER();

    sai_db_read_lock();

    status = mlnx_bridge_port_by_oid(key->key.object_id, &port);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed to lookup bridge port by oid %" PRIx64 "\n", key->key.object_id);
        goto out;
    }

    if (port->port_type != SAI_BRIDGE_PORT_TYPE_TUNNEL) {
        SX_LOG_ERR("Invalid bridge port type %d, SAI_BRIDGE_PORT_TYPE_TUNNEL is only supported\n",
                   port->port_type);
        status = SAI_STATUS_INVALID_PARAMETER;
        goto out;
    }

    status = mlnx_create_object(SAI_OBJECT_TYPE_TUNNEL, port->tunnel_idx, NULL, &value->oid);
    if (SAI_ERR(status)) {
        SX_LOG_EXIT();
        goto out;
    }

out:
    sai_db_unlock();
    SX_LOG_EXIT();
    return status;
}

/**
 * @brief Associated bridge id
 *
 * @type sai_object_id_t
 * @objects SAI_OBJECT_TYPE_BRIDGE
 * @flags CREATE_AND_SET
 * @default SAI_NULL_OBJECT_ID
 * @allownull true
 */
static sai_status_t mlnx_bridge_port_bridge_id_get(_In_ const sai_object_key_t   *key,
                                                   _Inout_ sai_attribute_value_t *value,
                                                   _In_ uint32_t                  attr_index,
                                                   _Inout_ vendor_cache_t        *cache,
                                                   void                          *arg)
{
    sai_status_t        status;
    mlnx_bridge_port_t *port;

    sai_db_read_lock();

    status = mlnx_bridge_port_by_oid(key->key.object_id, &port);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed to lookup bridge port object by oid %" PRIx64 "\n", key->key.object_id);
        goto out;
    }

    /* Just a trick as we do not allow to create .1Q bridge */
    if (port->bridge_id == mlnx_bridge_default_1q()) {
        value->oid = mlnx_bridge_default_1q_oid();
        goto out;
    }

    status = mlnx_create_bridge_1d_object(port->bridge_id, &value->oid);

out:
    sai_db_unlock();
    return status;
}

/**
 * @brief Associated bridge id
 *
 * @type sai_object_id_t
 * @objects SAI_OBJECT_TYPE_BRIDGE
 * @flags CREATE_AND_SET
 * @default SAI_NULL_OBJECT_ID
 * @allownull true
 */
static sai_status_t mlnx_bridge_port_bridge_id_set(_In_ const sai_object_key_t      *key,
                                                   _In_ const sai_attribute_value_t *value,
                                                   void                             *arg)
{
    mlnx_object_id_t    mlnx_bridge_id = {0};
    sx_bridge_id_t      sx_bridge_id;
    sx_status_t         sx_status;
    sai_bridge_type_t   br_type;
    sai_status_t        status;
    mlnx_bridge_port_t *port;

    status = sai_to_mlnx_object_id(SAI_OBJECT_TYPE_BRIDGE, value->oid, &mlnx_bridge_id);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed to convert bridge oid %" PRIx64 " to sx bridge id\n", value->oid);
        return status;
    }
    sx_bridge_id = mlnx_bridge_id.ext.bridge.sx_bridge_id;
    br_type = mlnx_bridge_id.field.sub_type;

    if (br_type != SAI_BRIDGE_TYPE_1D) {
        SX_LOG_ERR("Only .1D bridge is supported\n");
        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_db_write_lock();

    status = mlnx_bridge_port_by_oid(key->key.object_id, &port);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed to lookup bridge port object by oid %" PRIx64 "\n", key->key.object_id);
        goto out;
    }

    if (port->port_type == SAI_BRIDGE_PORT_TYPE_SUB_PORT) {
        /* Vport admin state needs to be down before deleting from a bridge */
        if (port->admin_state) {
            sx_status = sx_api_port_state_set(gh_sdk, port->logical, SX_PORT_ADMIN_STATUS_DOWN);
            if (SX_ERR(sx_status)) {
                SX_LOG_ERR("Failed to set vport %x admin state down - %s.\n", port->logical, SX_STATUS_MSG(sx_status));
                status = sdk_to_sai(sx_status);
                goto out;
            }
        }

        sx_status = sx_api_bridge_vport_set(gh_sdk, SX_ACCESS_CMD_DELETE, port->bridge_id, port->logical);
        if (SX_ERR(sx_status)) {
            SX_LOG_ERR("Failed to del vport %x from bridge %x - %s\n", port->logical, port->bridge_id,
                       SX_STATUS_MSG(sx_status));
            status = sdk_to_sai(sx_status);
            goto out;
        }

        sx_status = sx_api_bridge_vport_set(gh_sdk, SX_ACCESS_CMD_ADD, sx_bridge_id, port->logical);
        if (SX_ERR(sx_status)) {
            SX_LOG_ERR("Failed to add vport %x to bridge %x - %s\n", port->logical, sx_bridge_id,
                       SX_STATUS_MSG(sx_status));
            status = sdk_to_sai(sx_status);
            goto out;
        }

        if (port->admin_state) {
            sx_status = sx_api_port_state_set(gh_sdk, port->logical, SX_PORT_ADMIN_STATUS_UP);
            if (SX_ERR(sx_status)) {
                SX_LOG_ERR("Failed to set vport %x admin state up - %s.\n", port->logical, SX_STATUS_MSG(sx_status));
                status = sdk_to_sai(sx_status);
                goto out;
            }
        }
    } else if (port->port_type == SAI_BRIDGE_PORT_TYPE_1D_ROUTER) {
        mlnx_bridge_rif_t *br_rif;

        status = mlnx_bridge_rif_by_idx(port->rif_index, &br_rif);
        if (SAI_ERR(status)) {
            SX_LOG_ERR("Failed to lookup bridge rif by index %u\n", port->rif_index);
            goto out;
        }

        br_rif->intf_params.ifc.bridge.bridge = sx_bridge_id;

        sx_status = sx_api_router_interface_set(gh_sdk, SX_ACCESS_CMD_EDIT, br_rif->sx_data.vrf_id,
                                                &br_rif->intf_params, &br_rif->intf_attribs, &br_rif->sx_data.rif_id);
        if (SX_ERR(sx_status)) {
            SX_LOG_ERR("Failed to set router interface - %s.\n", SX_STATUS_MSG(sx_status));
            /* Reset to the old bridge id which is stored also in mlnx_bridge_port_t */
            br_rif->intf_params.ifc.bridge.bridge = port->bridge_id;
            status = sdk_to_sai(sx_status);
            goto out;
        }
    } else {
        SX_LOG_ERR("Bridge port set is only supported for sub-port or .1D router port type\n");
        status = SAI_STATUS_INVALID_PARAMETER;
        goto out;
    }

    port->bridge_id = sx_bridge_id;

out:
    sai_db_unlock();
    return status;
}

/**
 * @brief FDB Learning mode
 *
 * @type sai_bridge_port_fdb_learning_mode_t
 * @flags CREATE_AND_SET
 * @default SAI_BRIDGE_PORT_FDB_LEARNING_MODE_HW
 */
static sai_status_t mlnx_bridge_port_fdb_learning_mode_get(_In_ const sai_object_key_t   *key,
                                                           _Inout_ sai_attribute_value_t *value,
                                                           _In_ uint32_t                  attr_index,
                                                           _Inout_ vendor_cache_t        *cache,
                                                           void                          *arg)
{
    sx_fdb_learn_mode_t learn_mode;
    sx_port_log_id_t    log_port;
    sx_status_t         sx_status;
    sai_status_t        status;

    SX_LOG_ENTER();

    status = mlnx_bridge_port_sai_to_log_port(key->key.object_id, &log_port);
    if (SAI_ERR(status)) {
        return status;
    }

    sx_status = sx_api_fdb_port_learn_mode_get(gh_sdk, log_port, &learn_mode);
    if (SX_ERR(sx_status)) {
        SX_LOG_ERR("Failed to get port learning mode - %s.\n", SX_STATUS_MSG(sx_status));
        return sdk_to_sai(sx_status);
    }

    if (SX_FDB_LEARN_MODE_DONT_LEARN == learn_mode) {
        value->s32 = SAI_BRIDGE_PORT_FDB_LEARNING_MODE_DISABLE;
    } else if (SX_FDB_LEARN_MODE_CONTROL_LEARN == learn_mode) {
        value->s32 = SAI_BRIDGE_PORT_FDB_LEARNING_MODE_CPU_LOG;
    } else {
        value->s32 = SAI_BRIDGE_PORT_FDB_LEARNING_MODE_HW;
    }

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_bridge_port_fdb_learn_mode_sai_to_sx(_In_ sai_bridge_port_fdb_learning_mode_t mode,
                                                              _Out_ sx_fdb_learn_mode_t               *sx_mode)
{
    assert(sx_mode);

    switch (mode) {
    case SAI_BRIDGE_PORT_FDB_LEARNING_MODE_DISABLE:
        *sx_mode = SX_FDB_LEARN_MODE_DONT_LEARN;
        break;

    case SAI_BRIDGE_PORT_FDB_LEARNING_MODE_HW:
        *sx_mode = SX_FDB_LEARN_MODE_AUTO_LEARN;
        break;

    case SAI_BRIDGE_PORT_FDB_LEARNING_MODE_CPU_LOG:
        *sx_mode = SX_FDB_LEARN_MODE_CONTROL_LEARN;
        break;

    case SAI_BRIDGE_PORT_FDB_LEARNING_MODE_DROP:
    case SAI_BRIDGE_PORT_FDB_LEARNING_MODE_CPU_TRAP:
    case SAI_BRIDGE_PORT_FDB_LEARNING_MODE_FDB_NOTIFICATION:
        return SAI_STATUS_NOT_IMPLEMENTED;

    default:
        SX_LOG_ERR("Invalid port fdb learning mode %d\n", mode);
        return SAI_STATUS_INVALID_ATTR_VALUE_0;
    }

    return SAI_STATUS_SUCCESS;
}

/**
 * @brief FDB Learning mode
 *
 * @type sai_bridge_port_fdb_learning_mode_t
 * @flags CREATE_AND_SET
 * @default SAI_BRIDGE_PORT_FDB_LEARNING_MODE_HW
 */
static sai_status_t mlnx_bridge_port_fdb_learning_mode_set(_In_ const sai_object_key_t      *key,
                                                           _In_ const sai_attribute_value_t *value,
                                                           void                             *arg)
{
    sx_fdb_learn_mode_t learn_mode;
    sx_port_log_id_t    port_id;
    sx_status_t         sx_status;
    sai_status_t        status;

    SX_LOG_ENTER();

    status = mlnx_bridge_port_sai_to_log_port(key->key.object_id, &port_id);
    if (SAI_ERR(status)) {
        return status;
    }

    status = mlnx_bridge_port_fdb_learn_mode_sai_to_sx(value->s32, &learn_mode);
    if (SAI_ERR(status)) {
        return status;
    }

    sx_status = sx_api_fdb_port_learn_mode_set(gh_sdk, port_id, learn_mode);
    if (SX_ERR(sx_status)) {
        SX_LOG_ERR("Failed to set port %x learning mode - %s.\n", port_id, SX_STATUS_MSG(sx_status));
        return sdk_to_sai(sx_status);
    }

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

/**
 * @brief Maximum number of learned MAC addresses
 *
 * @type sai_uint32_t
 * @flags CREATE_AND_SET
 * @default 0
 */
static sai_status_t mlnx_bridge_port_max_learned_addresses_get(_In_ const sai_object_key_t   *key,
                                                               _Inout_ sai_attribute_value_t *value,
                                                               _In_ uint32_t                  attr_index,
                                                               _Inout_ vendor_cache_t        *cache,
                                                               void                          *arg)
{
    sai_status_t     status = SAI_STATUS_SUCCESS;
    sx_status_t      sx_status;
    sx_port_log_id_t sx_log_port_id;
    uint32_t         sx_limit = 0;

    SX_LOG_ENTER();

    status = mlnx_bridge_port_sai_to_log_port(key->key.object_id, &sx_log_port_id);
    if (SAI_ERR(status)) {
        goto out;
    }

    sx_status = sx_api_fdb_uc_limit_port_get(gh_sdk, sx_log_port_id, &sx_limit);
    if (SX_ERR(sx_status)) {
        SX_LOG_ERR("Failed to get FDB learning limit for port %x - %s\n", sx_log_port_id, SX_STATUS_MSG(sx_status));
        status = sdk_to_sai(status);
        goto out;
    }

    value->u32 = MLNX_FDB_LIMIT_SX_TO_SAI(sx_limit);

out:
    SX_LOG_EXIT();
    return status;
}

static sai_status_t mlnx_port_max_learned_addresses_set(_In_ sx_port_log_id_t sx_port, _In_ uint32_t limit)
{
    sx_status_t sx_status;
    uint32_t    sx_limit;

    sx_limit = MLNX_FDB_LIMIT_SAI_TO_SX(limit);

    sx_status = sx_api_fdb_uc_limit_port_set(gh_sdk, SX_ACCESS_CMD_SET, sx_port, sx_limit);
    if (SX_ERR(sx_status)) {
        SX_LOG_ERR("Failed to set FDB learning limit for port %x - %s\n", sx_port, SX_STATUS_MSG(sx_status));
        return sdk_to_sai(sx_status);
    }

    return SAI_STATUS_SUCCESS;
}

/**
 * @brief Maximum number of learned MAC addresses
 *
 * @type sai_uint32_t
 * @flags CREATE_AND_SET
 * @default 0
 */
static sai_status_t mlnx_bridge_port_max_learned_addresses_set(_In_ const sai_object_key_t      *key,
                                                               _In_ const sai_attribute_value_t *value,
                                                               void                             *arg)
{
    sai_status_t     status = SAI_STATUS_SUCCESS;
    sx_port_log_id_t sx_log_port_id;

    SX_LOG_ENTER();

    status = mlnx_bridge_port_sai_to_log_port(key->key.object_id, &sx_log_port_id);
    if (SAI_ERR(status)) {
        goto out;
    }

    status = mlnx_max_learned_addresses_value_validate(value->u32, 0);
    if (SAI_ERR(status)) {
        goto out;
    }

    status = mlnx_port_max_learned_addresses_set(sx_log_port_id, value->u32);
    if (SAI_ERR(status)) {
        goto out;
    }

out:
    SX_LOG_EXIT();
    return status;
}

/**
 * @brief Admin Mode.
 *
 * Before removing a bridge port, need to disable it by setting admin mode
 * to false, then flush the FDB entries, and then remove it.
 *
 * @type bool
 * @flags CREATE_AND_SET
 * @default false
 */
static sai_status_t mlnx_bridge_port_admin_state_get(_In_ const sai_object_key_t   *key,
                                                     _Inout_ sai_attribute_value_t *value,
                                                     _In_ uint32_t                  attr_index,
                                                     _Inout_ vendor_cache_t        *cache,
                                                     void                          *arg)
{
    mlnx_bridge_port_t *port;
    sai_status_t        status;

    SX_LOG_ENTER();

    sai_db_read_lock();

    status = mlnx_bridge_port_by_oid(key->key.object_id, &port);
    if (SAI_ERR(status)) {
        goto out;
    }

    value->booldata = port->admin_state;

out:
    sai_db_unlock();
    SX_LOG_EXIT();
    return status;
}

static sai_status_t mlnx_bridge_port_admin_state_set(_In_ const sai_object_key_t      *key,
                                                     _In_ const sai_attribute_value_t *value,
                                                     void                             *arg)
{
    sai_status_t        sai_status = SX_STATUS_SUCCESS;
    mlnx_bridge_port_t *bridge_port = NULL;

    SX_LOG_ENTER();

    sai_db_read_lock();

    sai_status = mlnx_bridge_port_by_oid(key->key.object_id, &bridge_port);
    if (SAI_ERR(sai_status)) {
        goto out;
    }

    sai_status = mlnx_bridge_port_admin_state_set_internal(bridge_port, value->booldata);
    if (SAI_ERR(sai_status)) {
        goto out;
    }

out:
    sai_db_unlock();
    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_bridge_port_tagging_mode_get(_In_ const sai_object_key_t   *key,
                                                      _Inout_ sai_attribute_value_t *value,
                                                      _In_ uint32_t                  attr_index,
                                                      _Inout_ vendor_cache_t        *cache,
                                                      void                          *arg)
{
    sai_status_t               status;
    sx_untagged_member_state_t sx_tagging_mode;
    mlnx_bridge_port_t        *bport;

    SX_LOG_ENTER();

    sai_db_read_lock();

    status = mlnx_bridge_port_by_oid(key->key.object_id, &bport);
    if (SAI_ERR(status)) {
        goto out;
    }

    if (bport->port_type != SAI_BRIDGE_PORT_TYPE_SUB_PORT) {
        SX_LOG_ERR("Tagging mode is only valid for sub-port\n");
        status = SAI_STATUS_FAILURE;
        goto out;
    }

    status = mlnx_vlan_log_port_tagging_get(bport->parent, bport->vlan_id, &sx_tagging_mode);
    if (SAI_ERR(status)) {
        goto out;
    }

    value->s32 = (sx_tagging_mode == SX_TAGGED_MEMBER) ?
                 SAI_BRIDGE_PORT_TAGGING_MODE_TAGGED : SAI_BRIDGE_PORT_TAGGING_MODE_UNTAGGED;

out:
    sai_db_unlock();
    SX_LOG_EXIT();
    return status;
}

static sai_status_t mlnx_bridge_port_tagging_mode_set(_In_ const sai_object_key_t      *key,
                                                      _In_ const sai_attribute_value_t *value,
                                                      void                             *arg)
{
    sai_status_t               status;
    sx_status_t                sx_status;
    sx_untagged_member_state_t sx_tagging_mode;
    sx_vlan_ports_t            sx_vlan_ports;
    mlnx_bridge_port_t        *bport;

    SX_LOG_ENTER();

    memset(&sx_vlan_ports, 0, sizeof(sx_vlan_ports));

    sai_db_write_lock();

    status = mlnx_bridge_port_by_oid(key->key.object_id, &bport);
    if (SAI_ERR(status)) {
        goto out;
    }

    if (bport->port_type != SAI_BRIDGE_PORT_TYPE_SUB_PORT) {
        SX_LOG_ERR("Tagging mode is only valid for sub-port\n");
        status = SAI_STATUS_FAILURE;
        goto out;
    }

    status = mlnx_bridge_port_tagging_sai_to_sx(value->s32, &sx_tagging_mode);
    if (SAI_ERR(status)) {
        goto out;
    }

    sx_vlan_ports.is_untagged = sx_tagging_mode;
    sx_vlan_ports.log_port = bport->parent;

    sx_status = sx_api_vlan_ports_set(gh_sdk, SX_ACCESS_CMD_ADD, DEFAULT_ETH_SWID, bport->vlan_id, &sx_vlan_ports, 1);
    if (SX_ERR(sx_status)) {
        SX_LOG_ERR("Failed to set sub-port (%x) {%x : %d} tagging mode to %d - %s\n", bport->logical,
                   bport->parent, bport->vlan_id, value->s32, SX_STATUS_MSG(sx_status));
        status = sdk_to_sai(sx_status);
        goto out;
    }

out:
    sai_db_unlock();
    SX_LOG_EXIT();
    return status;
}

static sai_status_t mlnx_bridge_port_ingr_filter_get(_In_ const sai_object_key_t   *key,
                                                     _Inout_ sai_attribute_value_t *value,
                                                     _In_ uint32_t                  attr_index,
                                                     _Inout_ vendor_cache_t        *cache,
                                                     void                          *arg)
{
    sai_status_t          status;
    sx_status_t           sx_status;
    sx_ingr_filter_mode_t sx_ingr_filter_mode;
    mlnx_bridge_port_t   *bport;

    SX_LOG_ENTER();

    sai_db_read_lock();

    status = mlnx_bridge_port_by_oid(key->key.object_id, &bport);
    if (SAI_ERR(status)) {
        goto out;
    }

    if (bport->port_type != SAI_BRIDGE_PORT_TYPE_PORT) {
        SX_LOG_ERR("Ingress filter is only supported for sub-port\n");
        status = SAI_STATUS_NOT_SUPPORTED;
        goto out;
    }

    sx_status = sx_api_vlan_port_ingr_filter_get(gh_sdk, bport->logical, &sx_ingr_filter_mode);
    if (SX_ERR(sx_status)) {
        SX_LOG_ERR("Failed to get port %x ingress filter - %s.\n", bport->logical, SX_STATUS_MSG(status));
        status = sdk_to_sai(status);
        goto out;
    }

    value->booldata = (SX_INGR_FILTER_DISABLE == sx_ingr_filter_mode) ? false : true;

out:
    sai_db_unlock();
    SX_LOG_EXIT();
    return status;
}
static sai_status_t mlnx_bridge_port_ingr_filter_set(_In_ const sai_object_key_t      *key,
                                                     _In_ const sai_attribute_value_t *value,
                                                     void                             *arg)
{
    sai_status_t          status;
    sx_status_t           sx_status;
    sx_ingr_filter_mode_t sx_ingr_filter_mode;
    mlnx_bridge_port_t   *bport;

    SX_LOG_ENTER();

    sai_db_read_lock();

    status = mlnx_bridge_port_by_oid(key->key.object_id, &bport);
    if (SAI_ERR(status)) {
        goto out;
    }

    if (bport->port_type != SAI_BRIDGE_PORT_TYPE_PORT) {
        SX_LOG_ERR("Ingress filter is only supported for bridge port type port\n");
        status = SAI_STATUS_NOT_SUPPORTED;
        goto out;
    }

    sx_ingr_filter_mode = value->booldata ? SX_INGR_FILTER_ENABLE : SX_INGR_FILTER_DISABLE;

    sx_status = sx_api_vlan_port_ingr_filter_set(gh_sdk, bport->logical, sx_ingr_filter_mode);
    if (SX_ERR(sx_status)) {
        SX_LOG_ERR("Failed to set port %x ingress filter - %s.\n", bport->logical, SX_STATUS_MSG(sx_status));
        status = sdk_to_sai(sx_status);
        goto out;
    }

out:
    sai_db_unlock();
    SX_LOG_EXIT();
    return status;
}

static sai_status_t mlnx_bridge_port_isolation_group_get(_In_ const sai_object_key_t   *key,
                                                         _Inout_ sai_attribute_value_t *value,
                                                         _In_ uint32_t                  attr_index,
                                                         _Inout_ vendor_cache_t        *cache,
                                                         void                          *arg)
{
    sai_status_t        status;
    mlnx_bridge_port_t *bport;

    SX_LOG_ENTER();

    sai_db_read_lock();

    status = mlnx_bridge_port_by_oid(key->key.object_id, &bport);
    if (SAI_ERR(status)) {
        goto out;
    }

    if (bport->port_type != SAI_BRIDGE_PORT_TYPE_PORT) {
        SX_LOG_ERR("Isolation group is only supported for bridge port type port\n");
        status = SAI_STATUS_NOT_SUPPORTED;
        goto out;
    }

    status = mlnx_get_port_isolation_group_impl(key->key.object_id, &value->oid);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed to get bridge port isolation group\n");
        goto out;
    }

out:
    sai_db_unlock();

    return status;
}

static sai_status_t mlnx_bridge_port_isolation_group_set(_In_ const sai_object_key_t      *key,
                                                         _In_ const sai_attribute_value_t *value,
                                                         void                             *arg)
{
    sai_status_t        status;
    mlnx_bridge_port_t *bport;

    SX_LOG_ENTER();

    sai_db_write_lock();

    status = mlnx_bridge_port_by_oid(key->key.object_id, &bport);
    if (SAI_ERR(status)) {
        goto out;
    }

    if (bport->port_type != SAI_BRIDGE_PORT_TYPE_PORT) {
        SX_LOG_ERR("Isolation group is only supported for bridge port type port\n");
        status = SAI_STATUS_NOT_SUPPORTED;
        goto out;
    }

    status = mlnx_set_port_isolation_group_impl(key->key.object_id, value->oid);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed to set bridge port isolation group\n");
        goto out;
    }

out:
    sai_db_unlock();

    return status;
}

/*
 * SAI DB should be locked
 */
static sai_status_t mlnx_sai_bridge_modify_1q_port_1d_mode_internal(_In_ mlnx_bridge_port_t *port, _In_ bool is_remove)
{
    sai_status_t     sai_status = SAI_STATUS_SUCCESS;
    sx_status_t      sx_status = SX_STATUS_SUCCESS;
    sx_bridge_id_t   sx_bridge_id = 0;
    sx_port_log_id_t sx_port_id = 0;
    sx_port_log_id_t sx_vport_id = 0;

    SX_LOG_ENTER();

    if (SAI_STATUS_SUCCESS !=
        (sai_status = mlnx_bridge_oid_to_id(mlnx_bridge_dummy_1d_oid(), &sx_bridge_id))) {
        goto out;
    }

    /* .1Q port logical field keeps port sx log id which is parent for vport */
    sx_port_id = port->logical;
    /* .1Q port parent field keeps vport sx log id */
    sx_vport_id = port->parent;

    if (is_remove) {
        /* dummy vport for .1Q port does not exist */
        if (sx_vport_id == (sx_port_log_id_t)-1) {
            goto out;
        }

        if (SX_STATUS_SUCCESS !=
            (sx_status = sx_api_bridge_vport_set(gh_sdk, SX_ACCESS_CMD_DELETE, sx_bridge_id, sx_vport_id))) {
            SX_LOG_ERR("Failed to del vport %x from bridge %x - %s\n", sx_vport_id, sx_bridge_id,
                       SX_STATUS_MSG(sx_status));
            sai_status = sdk_to_sai(sx_status);
            goto out;
        }

        if (SX_STATUS_SUCCESS !=
            (sx_status =
                 sx_api_port_vport_set(gh_sdk, SX_ACCESS_CMD_DELETE, sx_port_id, MLNX_SAI_DUMMY_1D_VLAN_ID,
                                       &sx_vport_id))) {
            SX_LOG_ERR("Failed to delete vport {%x : %d} - %s\n", sx_port_id, MLNX_SAI_DUMMY_1D_VLAN_ID,
                       SX_STATUS_MSG(sx_status));
            sai_status = sdk_to_sai(sx_status);
            goto out;
        }

        /* mark that vport is destroyed */
        port->parent = -1;
    } else {
        /* dummy vport for .1Q port is already created */
        if (sx_vport_id != (sx_port_log_id_t)-1) {
            goto out;
        }

        if (SX_STATUS_SUCCESS !=
            (sx_status =
                 sx_api_port_vport_set(gh_sdk, SX_ACCESS_CMD_ADD, sx_port_id, MLNX_SAI_DUMMY_1D_VLAN_ID,
                                       &sx_vport_id))) {
            SX_LOG_ERR("Failed to create vport {%x : %d} - %s\n", sx_port_id, MLNX_SAI_DUMMY_1D_VLAN_ID,
                       SX_STATUS_MSG(sx_status));
            sai_status = sdk_to_sai(sx_status);
            goto out;
        }

        if (SX_STATUS_SUCCESS !=
            (sx_status = sx_api_port_state_set(gh_sdk, sx_vport_id, SX_PORT_ADMIN_STATUS_DOWN))) {
            SX_LOG_ERR("Failed to set vport %x admin state down - %s.\n", sx_vport_id, SX_STATUS_MSG(sx_status));
            sai_status = sdk_to_sai(sx_status);
            goto out;
        }

        if (SX_STATUS_SUCCESS !=
            (sx_status = sx_api_bridge_vport_set(gh_sdk, SX_ACCESS_CMD_ADD, sx_bridge_id, sx_vport_id))) {
            SX_LOG_ERR("Failed to add vport %x to bridge %x - %s\n",
                       sx_vport_id,
                       sx_bridge_id,
                       SX_STATUS_MSG(sx_status));
            sai_status = sdk_to_sai(sx_status);
            goto out;
        }

        /* for .1Q port parent field is used to keep dummy vport id */
        port->parent = sx_vport_id;
    }

out:
    SX_LOG_EXIT();
    return sai_status;
}

/*
 * SAI DB should be locked
 */
static sai_status_t mlnx_bridge_port_admin_state_set_internal(_In_ mlnx_bridge_port_t *bridge_port, _In_ bool is_up)
{
    sai_status_t     sai_status = SAI_STATUS_SUCCESS;
    sx_status_t      sx_status = SX_STATUS_SUCCESS;
    sx_port_log_id_t sx_port_id = 0;

    SX_LOG_ENTER();

    assert(bridge_port);

    /* Trick to decrease physical port down time in case of .1Q port removal =>
     * on bridge port admin state down/up create/remove dummy vport and
     * move this dummy vport to/from dummy .1D bridge */
    if (SAI_BRIDGE_PORT_TYPE_PORT == bridge_port->port_type) {
        if (SAI_STATUS_SUCCESS != (sai_status =
                                       mlnx_sai_bridge_modify_1q_port_1d_mode_internal(bridge_port, is_up))) {
            goto bail;
        }
    } else if (SAI_BRIDGE_PORT_TYPE_SUB_PORT == bridge_port->port_type) {
        sx_port_id = bridge_port->logical;

        if (SX_STATUS_SUCCESS !=
            (sx_status = sx_api_port_state_set(gh_sdk,
                                               sx_port_id,
                                               is_up ? SX_PORT_ADMIN_STATUS_UP : SX_PORT_ADMIN_STATUS_DOWN))) {
            SX_LOG_ERR("Failed to set port admin state - %s.\n", SX_STATUS_MSG(sx_status));
            sai_status = sdk_to_sai(sx_status);
            goto bail;
        }
    }

    bridge_port->admin_state = is_up;

bail:
    SX_LOG_EXIT();
    return sai_status;
}

static sai_status_t mlnx_bridge_port_tagging_sai_to_sx(_In_ sai_bridge_port_tagging_mode_t sai_tagging_mode,
                                                       _Out_ sx_untagged_member_state_t   *sx_tagging_mode)
{
    assert(sx_tagging_mode);

    switch (sai_tagging_mode) {
    case SAI_BRIDGE_PORT_TAGGING_MODE_UNTAGGED:
        *sx_tagging_mode = SX_UNTAGGED_MEMBER;
        break;

    case SAI_BRIDGE_PORT_TAGGING_MODE_TAGGED:
        *sx_tagging_mode = SX_TAGGED_MEMBER;
        break;

    default:
        SX_LOG_ERR("Unexpected bridge port tagging mode - %d\n", sai_tagging_mode);
        return SAI_STATUS_FAILURE;
    }

    return SAI_STATUS_SUCCESS;
}

static void bridge_port_key_to_str(_In_ sai_object_id_t bridge_port_id, _Out_ char *key_str)
{
    mlnx_object_id_t mlnx_bridge_port = {0};
    sai_status_t     status;

    status = sai_to_mlnx_object_id(SAI_OBJECT_TYPE_BRIDGE_PORT, bridge_port_id, &mlnx_bridge_port);
    if (SAI_ERR(status)) {
        snprintf(key_str, MAX_KEY_STR_LEN, "invalid bridge port");
    } else {
        snprintf(key_str, MAX_KEY_STR_LEN, "bridge port idx %x", mlnx_bridge_port.id.u32);
    }
}

sai_status_t mlnx_bridge_port_availability_get(_In_ sai_object_id_t        switch_id,
                                               _In_ uint32_t               attr_count,
                                               _In_ const sai_attribute_t *attr_list,
                                               _Out_ uint64_t             *count)
{
    const rm_sdk_table_type_e table_type = RM_SDK_TABLE_TYPE_VPORTS_E;
    sx_status_t               sx_status;
    uint32_t                  ii, bports_left_1 = 0, bports_left_2 = 0;

    assert(count);

    for (ii = 0; ii < MAX_BRIDGE_PORTS; ii++) {
        if (!g_sai_db_ptr->bridge_ports_db[ii].is_present) {
            ++bports_left_1;
        }
    }

    sx_status = sx_api_rm_free_entries_by_type_get(gh_sdk, table_type, &bports_left_2);
    if (SX_ERR(sx_status)) {
        SX_LOG_ERR("Failed to get a number of free resources for sx table %d - %s\n", table_type,
                   SX_STATUS_MSG(sx_status));
        return sdk_to_sai(sx_status);
    }

    *count = (uint64_t)MIN(bports_left_1, bports_left_2);
    return SAI_STATUS_SUCCESS;
}

/**
 * @brief Create bridge port
 *
 * @param[out] bridge_port_id Bridge port ID
 * @param[in] switch_id Switch object id
 * @param[in] attr_count number of attributes
 * @param[in] attr_list array of attributes
 *
 * @return #SAI_STATUS_SUCCESS on success Failure status code on error
 */
static sai_status_t mlnx_create_bridge_port(_Out_ sai_object_id_t     * bridge_port_id,
                                            _In_ sai_object_id_t        switch_id,
                                            _In_ uint32_t               attr_count,
                                            _In_ const sai_attribute_t *attr_list)
{
    sai_status_t                 status = SAI_STATUS_NOT_IMPLEMENTED;
    char                         list_str[MAX_LIST_VALUE_STR_LEN];
    char                         key_str[MAX_KEY_STR_LEN];
    mlnx_object_id_t             mlnx_bridge_id = {0};
    mlnx_bridge_port_t          *bridge_port = NULL;
    mlnx_bridge_rif_t           *bridge_rif;
    sx_bridge_id_t               bridge_id;
    mlnx_shm_rm_array_idx_t      bridge_rm_idx;
    sx_status_t                  sx_status;
    sx_ingr_filter_mode_t        sx_ingr_filter_mode;
    sx_port_log_id_t             log_port;
    sx_port_log_id_t             vport_id = 0;
    sx_vlan_id_t                 vlan_id = 0;
    sx_untagged_member_state_t   sx_tagging_mode;
    sx_fdb_learn_mode_t          sx_learn_mode;
    const sai_attribute_value_t *attr_val, *max_learned_addresses = NULL, *ingress_filter = NULL;
    const sai_attribute_value_t *isolation_group = NULL;
    const sai_attribute_value_t *learn_mode = NULL;
    sai_bridge_port_type_t       bport_type;
    uint32_t                     attr_idx, max_learned_addresses_index, bridge_rif_idx, isolation_group_idx;
    bool                         admin_state;

    SX_LOG_ENTER();

    if (NULL == bridge_port_id) {
        SX_LOG_ERR("NULL bridge port ID param\n");
        return SAI_STATUS_INVALID_PARAMETER;
    }

    status = check_attribs_metadata(attr_count, attr_list, SAI_OBJECT_TYPE_BRIDGE_PORT, bridge_port_vendor_attribs,
                                    SAI_COMMON_API_CREATE);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed attribs check\n");
        return status;
    }

    status = check_attrs_port_type(NULL, attr_count, attr_list);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed attrs check\n");
        return status;
    }

    sai_attr_list_to_str(attr_count, attr_list, SAI_OBJECT_TYPE_BRIDGE_PORT, MAX_LIST_VALUE_STR_LEN, list_str);
    SX_LOG_NTC("Create bridge port, %s\n", list_str);

    sai_db_write_lock();

    status = find_attrib_in_list(attr_count, attr_list, SAI_BRIDGE_PORT_ATTR_BRIDGE_ID, &attr_val, &attr_idx);
    if (!SAI_ERR(status)) {
        status = sai_to_mlnx_object_id(SAI_OBJECT_TYPE_BRIDGE, attr_val->oid, &mlnx_bridge_id);
        if (SAI_ERR(status)) {
            SX_LOG_ERR("Failed parse bridge id %" PRIx64 "\n", attr_val->oid);
            goto out;
        }

        bridge_id = mlnx_bridge_id.ext.bridge.sx_bridge_id;
    } else {
        bridge_id = mlnx_bridge_default_1q();
    }


    status = find_attrib_in_list(attr_count, attr_list, SAI_BRIDGE_PORT_ATTR_TYPE, &attr_val, &attr_idx);
    assert(!SAI_ERR(status));

    bport_type = attr_val->s32;

    status = find_attrib_in_list(attr_count, attr_list, SAI_BRIDGE_PORT_ATTR_INGRESS_FILTERING,
                                 &ingress_filter, &attr_idx);
    if (!SAI_ERR(status)) {
        if (bport_type != SAI_BRIDGE_PORT_TYPE_PORT) {
            SX_LOG_ERR("Ingress filter is only supported for bridge port type port\n");
            status = SAI_STATUS_ATTR_NOT_SUPPORTED_0 + attr_idx;
            goto out;
        }
    }

    find_attrib_in_list(attr_count, attr_list, SAI_BRIDGE_PORT_ATTR_FDB_LEARNING_MODE, &learn_mode, &attr_idx);
    if (learn_mode &&
        ((bport_type == SAI_BRIDGE_PORT_TYPE_1D_ROUTER) || (bport_type == SAI_BRIDGE_PORT_TYPE_TUNNEL)) &&
        (learn_mode->s32 != SAI_BRIDGE_PORT_FDB_LEARNING_MODE_DISABLE)) {
        SX_LOG_ERR("The bridge port 1D_ROUTER and TUNNEL only supports SAI_BRIDGE_PORT_FDB_LEARNING_MODE_DISABLE\n");
        status = SAI_STATUS_ATTR_NOT_SUPPORTED_0 + attr_idx;
        goto out;
    }

    status = find_attrib_in_list(attr_count, attr_list, SAI_BRIDGE_PORT_ATTR_MAX_LEARNED_ADDRESSES,
                                 &max_learned_addresses, &max_learned_addresses_index);
    if (!SAI_ERR(status)) {
        status = mlnx_max_learned_addresses_value_validate(max_learned_addresses->u32, max_learned_addresses_index);
        if (SAI_ERR(status)) {
            goto out;
        }

        if ((bport_type != SAI_BRIDGE_PORT_TYPE_PORT) && (bport_type != SAI_BRIDGE_PORT_TYPE_SUB_PORT)) {
            SX_LOG_ERR("The SAI_BRIDGE_PORT_ATTR_MAX_LEARNED_ADDRESSES is only supported for PORT and SUB_PORT\n");
            status = SAI_STATUS_ATTR_NOT_SUPPORTED_0 + max_learned_addresses_index;
            goto out;
        }
    }

    status = find_attrib_in_list(attr_count, attr_list, SAI_BRIDGE_PORT_ATTR_ISOLATION_GROUP, &isolation_group,
                                 &isolation_group_idx);
    if (!SAI_ERR(status)) {
        if (bport_type != SAI_BRIDGE_PORT_TYPE_PORT) {
            SX_LOG_ERR("The SAI_BRIDGE_PORT_ATTR_ISOLATION_GROUP is only supported for PORT type\n");
            status = SAI_STATUS_ATTR_NOT_SUPPORTED_0 + isolation_group_idx;
            goto out;
        }
    }

    status = mlnx_bridge_port_add(bridge_id, bport_type, &bridge_port);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed to allocate bridge port entry\n");
        goto out;
    }

    switch (bport_type) {
    case SAI_BRIDGE_PORT_TYPE_PORT:
        status = find_attrib_in_list(attr_count, attr_list, SAI_BRIDGE_PORT_ATTR_PORT_ID, &attr_val, &attr_idx);
        if (SAI_ERR(status)) {
            SX_LOG_ERR("Missing mandatory SAI_BRIDGE_PORT_ATTR_PORT_ID attr\n");
            status = SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
            goto out;
        }

        status = mlnx_object_to_log_port(attr_val->oid, &log_port);
        if (SAI_ERR(status)) {
            SX_LOG_ERR("Failed to convert port oid %" PRIx64 " to log port\n", attr_val->oid);
            goto out;
        }

        if (mlnx_log_port_is_cpu(log_port)) {
            SX_LOG_ERR("Invalid port id - CPU port\n");
            status = SAI_STATUS_INVALID_ATTR_VALUE_0 + attr_idx;
            goto out;
        }

        if (mlnx_bridge_port_in_1q_by_log(log_port)) {
            SX_LOG_ERR("Port is already in .1Q bridge\n");
            status = SAI_STATUS_INVALID_PARAMETER;
            goto out;
        }

        if (ingress_filter) {
            sx_ingr_filter_mode = ingress_filter->booldata ? SX_INGR_FILTER_ENABLE : SX_INGR_FILTER_DISABLE;

            sx_status = sx_api_vlan_port_ingr_filter_set(gh_sdk, log_port, sx_ingr_filter_mode);
            if (SX_ERR(sx_status)) {
                SX_LOG_ERR("Failed to set port %x ingress filter - %s.\n", log_port, SX_STATUS_MSG(sx_status));
                status = sdk_to_sai(sx_status);
                goto out;
            }
        }

        bridge_port->logical = log_port;
        /* for .1Q bridge port used internally */
        bridge_port->parent = -1;
        bridge_port->vlan_id = 0;
        break;

    case SAI_BRIDGE_PORT_TYPE_SUB_PORT:
        if (bridge_id == mlnx_bridge_default_1q()) {
            SX_LOG_ERR("Bridge sub-port requires .1D bridge port\n");
            status = SAI_STATUS_INVALID_PARAMETER;
            goto out;
        }

        status = find_attrib_in_list(attr_count, attr_list, SAI_BRIDGE_PORT_ATTR_PORT_ID, &attr_val, &attr_idx);
        if (SAI_ERR(status)) {
            SX_LOG_ERR("Missing mandatory SAI_BRIDGE_PORT_ATTR_PORT_ID attr\n");
            status = SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
            goto out;
        }

        status = mlnx_object_to_log_port(attr_val->oid, &log_port);
        if (SAI_ERR(status)) {
            SX_LOG_ERR("Failed to convert port oid %" PRIx64 " to log port\n", attr_val->oid);
            goto out;
        }

        if (mlnx_bridge_port_in_1q_by_log(log_port)) {
            SX_LOG_ERR("Port is already in .1Q bridge\n");
            status = SAI_STATUS_INVALID_PARAMETER;
            goto out;
        }

        if (mlnx_log_port_is_cpu(log_port)) {
            SX_LOG_ERR("Invalid port id - CPU port\n");
            status = SAI_STATUS_INVALID_ATTR_VALUE_0 + attr_idx;
            goto out;
        }

        status = find_attrib_in_list(attr_count, attr_list, SAI_BRIDGE_PORT_ATTR_VLAN_ID, &attr_val, &attr_idx);
        if (SAI_ERR(status)) {
            SX_LOG_ERR("Missing mandatory SAI_BRIDGE_PORT_ATTR_VLAN_ID attr\n");
            status = SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
            goto out;
        }
        vlan_id = attr_val->u16;

        sx_tagging_mode = SX_TAGGED_MEMBER;
        status = find_attrib_in_list(attr_count,
                                     attr_list,
                                     SAI_BRIDGE_PORT_ATTR_TAGGING_MODE,
                                     &attr_val,
                                     &attr_idx);
        if (!SAI_ERR(status)) {
            status = mlnx_bridge_port_tagging_sai_to_sx(attr_val->s32, &sx_tagging_mode);
            if (SAI_ERR(status)) {
                goto out;
            }
        }

        status = mlnx_bridge_sx_vport_create(log_port, vlan_id, sx_tagging_mode, &vport_id);
        if (SAI_ERR(status)) {
            goto out;
        }

        sx_status = sx_api_bridge_vport_set(gh_sdk, SX_ACCESS_CMD_ADD, bridge_id, vport_id);
        if (SX_ERR(sx_status)) {
            SX_LOG_ERR("Failed to add vport %x to bridge %x - %s\n", vport_id, bridge_id, SX_STATUS_MSG(sx_status));
            status = sdk_to_sai(sx_status);
            goto out;
        }

        bridge_port->logical = vport_id;
        bridge_port->parent = log_port;
        bridge_port->vlan_id = vlan_id;

        status = mlnx_bridge_sx_to_db_idx(bridge_id, &bridge_rm_idx);
        if (SAI_ERR(status)) {
            goto out;
        }

        status = mlnx_fid_flood_ctrl_port_event_handle(bridge_id, &mlnx_bridge_1d_by_rm_idx(bridge_rm_idx)->flood_data,
                                                       &bridge_port->logical, 1, MLNX_PORT_EVENT_ADD);
        if (SAI_ERR(status)) {
            goto out;
        }
        break;

    case SAI_BRIDGE_PORT_TYPE_TUNNEL:
        status = find_attrib_in_list(attr_count, attr_list, SAI_BRIDGE_PORT_ATTR_TUNNEL_ID, &attr_val, &attr_idx);
        if (SAI_ERR(status)) {
            SX_LOG_ERR("Missing mandatory SAI_BRIDGE_PORT_ATTR_TUNNEL_ID attr\n");
            status = SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
            goto out;
        }

        status = mlnx_get_sai_tunnel_db_idx(attr_val->oid, &bridge_port->tunnel_idx);
        if (SAI_ERR(status)) {
            SX_LOG_ERR("Failed to convert oid to mlnx object id\n");
            goto out;
        }
        break;

    case SAI_BRIDGE_PORT_TYPE_1D_ROUTER:
        if (bridge_id == mlnx_bridge_default_1q()) {
            SX_LOG_ERR("Bridge port .1D router requires .1D bridge\n");
            status = SAI_STATUS_INVALID_PARAMETER;
            goto out;
        }

        status = find_attrib_in_list(attr_count, attr_list, SAI_BRIDGE_PORT_ATTR_RIF_ID, &attr_val, &attr_idx);
        if (SAI_ERR(status)) {
            SX_LOG_ERR("Missing mandatory SAI_BRIDGE_PORT_ATTR_RIF_ID attr\n");
            status = SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
            goto out;
        }

        status = mlnx_rif_oid_to_bridge_rif(attr_val->oid, &bridge_rif_idx);
        if (SAI_ERR(status)) {
            SX_LOG_ERR("Failed to fetch data from rif oid %lx\n", attr_val->oid);
            goto out;
        }

        status = mlnx_bridge_rif_by_idx(bridge_rif_idx, &bridge_rif);
        if (SAI_ERR(status)) {
            SX_LOG_ERR("Failed to lookup bridge rif by index %u\n", bridge_rif_idx);
            goto out;
        }

        if (bridge_rif->is_created) {
            SX_LOG_ERR("Bridge bridge port for rif %lx is already created\n", attr_val->oid);
            status = SAI_STATUS_ITEM_ALREADY_EXISTS;
            goto out;
        }

        bridge_rif->intf_params.ifc.bridge.swid = DEFAULT_ETH_SWID;
        bridge_rif->intf_params.ifc.bridge.bridge = bridge_id;

        status = mlnx_rif_sx_init(bridge_rif->sx_data.vrf_id, &bridge_rif->intf_params, &bridge_rif->intf_attribs,
                                  &bridge_rif->sx_data.rif_id, &bridge_rif->sx_data.counter);
        if (SAI_ERR(status)) {
            SX_LOG_ERR("Failed to init bridge rif\n");
            goto out;
        }

        sx_status = sx_api_router_interface_state_set(gh_sdk, bridge_rif->sx_data.rif_id, &bridge_rif->intf_state);
        if (SX_ERR(sx_status)) {
            SX_LOG_ERR("Failed to set bridge router interface state - %s.\n", SX_STATUS_MSG(sx_status));
            status = sdk_to_sai(sx_status);
            goto out;
        }

        bridge_rif->is_created = true;
        bridge_port->rif_index = bridge_rif->index;
        break;

    default:
        SX_LOG_ERR("Unsupported bridge port type %d\n", attr_val->s32);
        status = SAI_STATUS_INVALID_ATTR_VALUE_0 + attr_idx;
        goto out;
    }

    status = find_attrib_in_list(attr_count, attr_list, SAI_BRIDGE_PORT_ATTR_ADMIN_STATE, &attr_val, &attr_idx);
    if (SAI_ERR(status)) {
        admin_state = false;
    } else {
        admin_state = attr_val->booldata;
    }

    if (max_learned_addresses) {
        status = mlnx_port_max_learned_addresses_set(bridge_port->logical, max_learned_addresses->u32);
        if (SAI_ERR(status)) {
            goto out;
        }
    }

    if (learn_mode && ((bport_type == SAI_BRIDGE_PORT_TYPE_PORT) || (bport_type == SAI_BRIDGE_PORT_TYPE_SUB_PORT))) {
        status = mlnx_bridge_port_fdb_learn_mode_sai_to_sx(learn_mode->s32, &sx_learn_mode);
        if (SAI_ERR(status)) {
            goto out;
        }

        sx_status = sx_api_fdb_port_learn_mode_set(gh_sdk, bridge_port->logical, sx_learn_mode);
        if (SX_ERR(sx_status)) {
            SX_LOG_ERR("Failed to set port %x learning mode - %s.\n", bridge_port->logical, SX_STATUS_MSG(sx_status));
            status = sdk_to_sai(sx_status);
            goto out;
        }
    }

    status = mlnx_bridge_port_admin_state_set_internal(bridge_port, admin_state);
    if (SAI_ERR(status)) {
        goto out;
    }

    status = mlnx_bridge_port_to_oid(bridge_port, bridge_port_id);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed to convert bridge port to oid\n");
        goto out;
    }

    if (isolation_group != NULL) {
        status = mlnx_set_port_isolation_group_impl(*bridge_port_id, isolation_group->oid);
        if (SAI_ERR(status)) {
            SX_LOG_ERR("Failed to set bridge port isolation group\n");
            goto out;
        }
    }
    bridge_port_key_to_str(*bridge_port_id, key_str);
    SX_LOG_NTC("Created %s\n", key_str);

out:
    /* rollback */
    if (SAI_ERR(status)) {
        if (bridge_port) {
            mlnx_bridge_port_del(bridge_port);
        }
        if (vport_id) {
            sx_api_port_vport_set(gh_sdk, SX_ACCESS_CMD_DELETE, log_port, vlan_id, &vport_id);
        }
    }

    sai_db_unlock();
    SX_LOG_EXIT();
    return status;
}

static sai_status_t mlnx_bridge_port_in_use_check(mlnx_bridge_port_t *port)
{
    sai_status_t        status;
    mlnx_port_config_t *underlay_port_cfg;

    if (port->vlans) {
        SX_LOG_ERR("Failed remove bridge port - is used by VLAN members (%u)\n", port->vlans);
        return SAI_STATUS_OBJECT_IN_USE;
    }
    if (port->fdbs) {
        SX_LOG_ERR("Failed remove bridge port - is used by FDB actions (%u)\n", port->fdbs);
        return SAI_STATUS_OBJECT_IN_USE;
    }
    if (port->stps) {
        SX_LOG_ERR("Failed remove bridge port - is used by STP ports (%u)\n", port->stps);
        return SAI_STATUS_OBJECT_IN_USE;
    }
    if (port->pbs_entry.ref_counter > 0) {
        SX_LOG_ERR("Failed remove bridge port - is used by ACL Entry redirect action (%d refs)\n",
                   port->pbs_entry.ref_counter);
        return SAI_STATUS_OBJECT_IN_USE;
    }
    if (port->l2mc_group_ref > 0) {
        SX_LOG_ERR("Failed remove bridge port - is used as L2MC group member (%d refs)\n",
                   port->l2mc_group_ref);
        return SAI_STATUS_OBJECT_IN_USE;
    }

    if ((port->port_type == SAI_BRIDGE_PORT_TYPE_PORT) && is_isolation_group_in_use()) {
        status = mlnx_port_by_log_id(port->logical, &underlay_port_cfg);
        if (SAI_ERR(status)) {
            SX_LOG_ERR("Failed to get port config by log id\n");
            return status;
        }

        if (underlay_port_cfg->isolation_group_bridge_port_refcount > 0) {
            SX_LOG_ERR("Failed remove bridge port - is used as isolation group member\n");
            return SAI_STATUS_OBJECT_IN_USE;
        }
    }

    return SAI_STATUS_SUCCESS;
}

/**
 * @brief Remove bridge port
 *
 * @param[in] bridge_port_id Bridge port ID
 *
 * @return #SAI_STATUS_SUCCESS on success Failure status code on error
 */
static sai_status_t mlnx_remove_bridge_port(_In_ sai_object_id_t bridge_port_id)
{
    char                    key_str[MAX_KEY_STR_LEN];
    sx_status_t             sx_status;
    sai_status_t            status;
    mlnx_shm_rm_array_idx_t bridge_rm_idx;
    mlnx_bridge_rif_t      *bridge_rif;
    mlnx_bridge_port_t     *port;
    sai_object_id_t         isolation_group;

    SX_LOG_ENTER();

    bridge_port_key_to_str(bridge_port_id, key_str);
    SX_LOG_NTC("Remove %s\n", key_str);

    sai_db_write_lock();

    status = mlnx_bridge_port_by_oid(bridge_port_id, &port);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed to lookup bridge port\n");
        goto out;
    }

    status = mlnx_bridge_port_in_use_check(port);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Bridge port is in use\n");
        goto out;
    }

    switch (port->port_type) {
    case SAI_BRIDGE_PORT_TYPE_PORT:
        status = mlnx_get_port_isolation_group_impl(bridge_port_id, &isolation_group);
        if (SAI_ERR(status)) {
            SX_LOG_ERR("Failed to get sai isolation group\n");
            goto out;
        }

        if (isolation_group != SAI_NULL_OBJECT_ID) {
            status = mlnx_set_port_isolation_group_impl(bridge_port_id, SAI_NULL_OBJECT_ID);
            if (SAI_ERR(status)) {
                SX_LOG_ERR("Failed to unbind isolation group\n");
                goto out;
            }
        }

        /* move dummy vport mapped to .1Q port out of .1D bridge and destroy this vport */
        if (SAI_STATUS_SUCCESS !=
            (status =
                 mlnx_sai_bridge_modify_1q_port_1d_mode_internal(port, true))) {
            goto out;
        }
        break;

    case SAI_BRIDGE_PORT_TYPE_SUB_PORT:
        sx_status = sx_api_bridge_vport_set(gh_sdk, SX_ACCESS_CMD_DELETE, port->bridge_id, port->logical);
        if (SX_ERR(sx_status)) {
            SX_LOG_ERR("Failed to del vport %x from bridge %x - %s\n", port->logical, port->bridge_id,
                       SX_STATUS_MSG(sx_status));
            status = sdk_to_sai(sx_status);
            goto out;
        }

        status = mlnx_bridge_sx_vport_delete(port->parent, port->vlan_id, port->logical);
        if (SAI_ERR(status)) {
            goto out;
        }
        break;

    case SAI_BRIDGE_PORT_TYPE_1D_ROUTER:
        status = mlnx_bridge_rif_by_idx(port->rif_index, &bridge_rif);
        if (SAI_ERR(status)) {
            SX_LOG_ERR("Failed to lookup bridge rif by index %u\n", port->rif_index);
            goto out;
        }

        status = mlnx_rif_sx_deinit(&bridge_rif->sx_data);
        if (SAI_ERR(status)) {
            goto out;
        }

        bridge_rif->intf_params.ifc.bridge.bridge = SX_BRIDGE_ID_INVALID;
        bridge_rif->is_created = false;
        bridge_rif->sx_data.rif_id = 0;
        break;

    default:
        break;
    }

    status = mlnx_bridge_port_del(port);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed to remove bridge port\n");
        goto out;
    }

    if (port->port_type == SAI_BRIDGE_PORT_TYPE_SUB_PORT) {
        status = mlnx_bridge_sx_to_db_idx(port->bridge_id, &bridge_rm_idx);
        if (SAI_ERR(status)) {
            goto out;
        }

        status = mlnx_fid_flood_ctrl_port_event_handle(port->bridge_id,
                                                       &mlnx_bridge_1d_by_rm_idx(bridge_rm_idx)->flood_data,
                                                       &port->logical, 1, MLNX_PORT_EVENT_DELETE);
        if (SAI_ERR(status)) {
            goto out;
        }
    }

out:
    sai_db_unlock();
    SX_LOG_EXIT();
    return status;
}

/**
 * @brief Set attribute for bridge port
 *
 * @param[in] bridge_port_id Bridge port ID
 * @param[in] attr attribute to set
 *
 * @return #SAI_STATUS_SUCCESS on success Failure status code on error
 */
static sai_status_t mlnx_set_bridge_port_attribute(_In_ sai_object_id_t        bridge_port_id,
                                                   _In_ const sai_attribute_t *attr)
{
    const sai_object_key_t key = { .key.object_id = bridge_port_id };
    char                   key_str[MAX_KEY_STR_LEN];
    sai_status_t           status;

    SX_LOG_ENTER();

    bridge_port_key_to_str(bridge_port_id, key_str);

    status = check_attrs_port_type(&key, 1, attr);
    if (SAI_ERR(status)) {
        return status;
    }

    return sai_set_attribute(&key, key_str, SAI_OBJECT_TYPE_BRIDGE_PORT, bridge_port_vendor_attribs, attr);
}

/**
 * @brief Get attributes of bridge port
 *
 * @param[in] bridge_port_id Bridge port ID
 * @param[in] attr_count number of attributes
 * @param[inout] attr_list array of attributes
 *
 * @return #SAI_STATUS_SUCCESS on success Failure status code on error
 */
static sai_status_t mlnx_get_bridge_port_attribute(_In_ sai_object_id_t     bridge_port_id,
                                                   _In_ uint32_t            attr_count,
                                                   _Inout_ sai_attribute_t *attr_list)
{
    const sai_object_key_t key = { .key.object_id = bridge_port_id };
    char                   key_str[MAX_KEY_STR_LEN];

    SX_LOG_ENTER();

    bridge_port_key_to_str(bridge_port_id, key_str);
    return sai_get_attributes(&key, key_str, SAI_OBJECT_TYPE_BRIDGE_PORT, bridge_port_vendor_attribs,
                              attr_count, attr_list);
}

sai_status_t mlnx_bridge_log_set(sx_verbosity_level_t level)
{
    LOG_VAR_NAME(__MODULE__) = level;

    if (gh_sdk) {
        return sdk_to_sai(sx_api_bridge_log_verbosity_level_set(gh_sdk, SX_LOG_VERBOSITY_BOTH, level, level));
    } else {
        return SAI_STATUS_SUCCESS;
    }
}

/* Need to be guarded by lock */
static sai_status_t mlnx_sdk_bridge_create(_Out_ sx_bridge_id_t *sx_bridge_id)
{
    sx_status_t      sx_status = SX_STATUS_ERROR;
    sai_status_t     sai_status = SAI_STATUS_FAILURE;
    sx_vlan_attrib_t vlan_attrib_p;

    SX_LOG_ENTER();

    sx_status = sx_api_bridge_set(gh_sdk, SX_ACCESS_CMD_CREATE, sx_bridge_id);
    if (SX_ERR(sx_status)) {
        sai_status = sdk_to_sai(sx_status);
        SX_LOG_ERR("Failed to create bridge - %s\n", SX_STATUS_MSG(sx_status));
        SX_LOG_EXIT();
        return sai_status;
    }

    memset(&vlan_attrib_p, 0, sizeof(vlan_attrib_p));
    vlan_attrib_p.flood_to_router = true;

    if (g_sai_db_ptr->fx_initialized) {
        sx_status = sx_api_vlan_attrib_set(gh_sdk, *sx_bridge_id, &vlan_attrib_p);
        if (SX_ERR(sx_status)) {
            sai_status = sdk_to_sai(sx_status);
            SX_LOG_ERR("Error setting vlan attribute for fid %d: %s\n",
                       *sx_bridge_id, SX_STATUS_MSG(sx_status));
            SX_LOG_EXIT();
            return sai_status;
        }
    }

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

sai_status_t mlnx_bridge_init(void)
{
    mlnx_bridge_port_t *router_port;
    sx_bridge_id_t      bridge_id;
    mlnx_port_config_t *port;
    sai_status_t        status;
    uint32_t            ii;
    const bool          is_warmboot_init_stage = (BOOT_TYPE_WARM == g_sai_db_ptr->boot_type) &&
                                                 (!g_sai_db_ptr->issu_end_called);

    sai_db_write_lock();

    status = mlnx_sdk_bridge_create(&bridge_id);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed to create default .1Q bridge\n");
        goto out;
    }

    mlnx_port_phy_foreach(port, ii) {
        mlnx_bridge_port_t *bridge_port;

        status = mlnx_bridge_port_add(bridge_id, SAI_BRIDGE_PORT_TYPE_PORT, &bridge_port);
        if (SAI_ERR(status)) {
            SX_LOG_ERR("Failed to add port %x to default bridge\n", port->logical);
            goto out;
        }

        bridge_port->logical = port->logical;
        bridge_port->admin_state = true;

        /* for .1Q bridge port used internally */
        bridge_port->parent = -1;
        bridge_port->vlan_id = 0;

        if (!is_warmboot_init_stage ||
            (!(port->before_issu_lag_id) && port->sdk_port_added)) {
            status = mlnx_vlan_port_add(DEFAULT_VLAN, SAI_VLAN_TAGGING_MODE_UNTAGGED, bridge_port);
            if (SAI_ERR(status)) {
                SX_LOG_ERR("Failed to add bridge port to default vlan\n");
                goto out;
            }
        }
    }

    status = mlnx_bridge_port_add(bridge_id, SAI_BRIDGE_PORT_TYPE_1Q_ROUTER, &router_port);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed to create router .1Q bridge port\n");
        goto out;
    }
    router_port->admin_state = true;

    g_sai_db_ptr->sx_bridge_id = bridge_id;

    status = mlnx_create_bridge_object(SAI_BRIDGE_TYPE_1Q, MLNX_SHM_RM_ARRAY_IDX_UNINITIALIZED,
                                       bridge_id, &g_sai_db_ptr->default_1q_bridge_oid);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed to create default .1Q bridge oid\n");
        goto out;
    }

    if (SAI_STATUS_SUCCESS !=
        (status = mlnx_sdk_bridge_create(&bridge_id))) {
        SX_LOG_ERR("Failed to create dummy .1D bridge\n");
        goto out;
    }

    if (SAI_STATUS_SUCCESS !=
        (status = mlnx_create_bridge_object(SAI_BRIDGE_TYPE_1D, MLNX_SHM_RM_ARRAY_IDX_UNINITIALIZED,
                                            bridge_id, &g_sai_db_ptr->dummy_1d_bridge_oid))) {
        SX_LOG_ERR("Failed to create dummy .1D bridge oid\n");
        goto out;
    }
out:
    sai_db_unlock();
    return status;
}

/**
 * @brief Get bridge statistics counters.
 *
 * @param[in] bridge_id Bridge id
 * @param[in] number_of_counters Number of counters in the array
 * @param[in] counter_ids Specifies the array of counter ids
 * @param[out] counters Array of resulting counter values.
 *
 * @return #SAI_STATUS_SUCCESS on success, failure status code on error
 */
static sai_status_t mlnx_get_bridge_stats(_In_ sai_object_id_t      bridge_id,
                                          _In_ uint32_t             number_of_counters,
                                          _In_ const sai_stat_id_t *counter_ids,
                                          _Out_ uint64_t           *counters)
{
    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * @brief Get bridge statistics counters extended.
 *
 * @param[in] bridge_id Bridge id
 * @param[in] number_of_counters Number of counters in the array
 * @param[in] counter_ids Specifies the array of counter ids
 * @param[in] mode Statistics mode
 * @param[out] counters Array of resulting counter values.
 *
 * @return #SAI_STATUS_SUCCESS on success, failure status code on error
 */
sai_status_t mlnx_get_bridge_stats_ext(_In_ sai_object_id_t      bridge_id,
                                       _In_ uint32_t             number_of_counters,
                                       _In_ const sai_stat_id_t *counter_ids,
                                       _In_ sai_stats_mode_t     mode,
                                       _Out_ uint64_t           *counters)
{
    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * @brief Clear bridge statistics counters.
 *
 * @param[in] bridge_id Bridge id
 * @param[in] number_of_counters Number of counters in the array
 * @param[in] counter_ids Specifies the array of counter ids
 *
 * @return #SAI_STATUS_SUCCESS on success, failure status code on error
 */
static sai_status_t mlnx_clear_bridge_stats(_In_ sai_object_id_t      bridge_id,
                                            _In_ uint32_t             number_of_counters,
                                            _In_ const sai_stat_id_t *counter_ids)
{
    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * @brief Get bridge port statistics counters extended.
 *
 * @param[in] bridge_port_id Bridge port id
 * @param[in] number_of_counters Number of counters in the array
 * @param[in] counter_ids Specifies the array of counter ids
 * @param[in] mode Statistics mode
 * @param[out] counters Array of resulting counter values.
 *
 * @return #SAI_STATUS_SUCCESS on success, failure status code on error
 */
sai_status_t mlnx_get_bridge_port_stats_ext(_In_ sai_object_id_t      bridge_port_id,
                                            _In_ uint32_t             number_of_counters,
                                            _In_ const sai_stat_id_t *counter_ids,
                                            _In_ sai_stats_mode_t     mode,
                                            _Out_ uint64_t           *counters)
{
    sai_status_t            status;
    sx_status_t             sx_status;
    sx_port_cntr_rfc_2863_t cntr_rfc_2863;
    mlnx_bridge_port_t     *bport;
    char                    key_str[MAX_KEY_STR_LEN];
    uint32_t                ii;
    sx_access_cmd_t         cmd;

    SX_LOG_ENTER();

    memset(&cntr_rfc_2863, 0, sizeof(cntr_rfc_2863));

    if (NULL == counter_ids) {
        SX_LOG_ERR("NULL counter ids array param\n");
        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (NULL == counters) {
        SX_LOG_ERR("NULL counters array param\n");
        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (SAI_STATUS_SUCCESS !=
        (status = mlnx_translate_sai_stats_mode_to_sdk(mode, &cmd))) {
        return status;
    }

    bridge_port_key_to_str(bridge_port_id, key_str);
    SX_LOG_DBG("Get bridge port stats %s\n", key_str);

    sai_db_read_lock();

    status = mlnx_bridge_port_by_oid(bridge_port_id, &bport);
    if (SAI_ERR(status)) {
        goto out;
    }

    /*
     * SDK doesn't support counters for VPORT, so the only supported bridge port type is SAI_BRIDGE_PORT_TYPE_PORT
     */
    sx_status = sx_api_port_counter_rfc_2863_get(gh_sdk, cmd, bport->logical, &cntr_rfc_2863);
    if (SX_ERR(sx_status)) {
        SX_LOG_ERR("Failed to get port [%x] rfc 2863 counters - %s.\n", bport->logical, SX_STATUS_MSG(sx_status));
        status = sdk_to_sai(sx_status);
        goto out;
    }

    for (ii = 0; ii < number_of_counters; ii++) {
        switch (counter_ids[ii]) {
        case SAI_BRIDGE_PORT_STAT_IN_OCTETS:
            counters[ii] = cntr_rfc_2863.if_in_octets;
            break;

        case SAI_BRIDGE_PORT_STAT_OUT_OCTETS:
            counters[ii] = cntr_rfc_2863.if_out_octets;
            break;

        case SAI_BRIDGE_PORT_STAT_IN_PACKETS:
            counters[ii] = cntr_rfc_2863.if_in_ucast_pkts + cntr_rfc_2863.if_in_broadcast_pkts +
                           cntr_rfc_2863.if_in_multicast_pkts;
            break;

        case SAI_BRIDGE_PORT_STAT_OUT_PACKETS:
            counters[ii] = cntr_rfc_2863.if_out_ucast_pkts + cntr_rfc_2863.if_out_broadcast_pkts +
                           cntr_rfc_2863.if_out_multicast_pkts;
            break;

        default:
            SX_LOG_ERR("Unexpected type of counter - %d\n", counter_ids[ii]);
            status = SAI_STATUS_INVALID_ATTRIBUTE_0 + ii;
            goto out;
        }
    }

out:
    sai_db_unlock();
    SX_LOG_EXIT();
    return status;
}

/**
 * @brief Get bridge port statistics counters.
 *
 * @param[in] bridge_port_id Bridge port id
 * @param[in] number_of_counters Number of counters in the array
 * @param[in] counter_ids Specifies the array of counter ids
 * @param[out] counters Array of resulting counter values.
 *
 * @return #SAI_STATUS_SUCCESS on success, failure status code on error
 */
static sai_status_t mlnx_get_bridge_port_stats(_In_ sai_object_id_t      bridge_port_id,
                                               _In_ uint32_t             number_of_counters,
                                               _In_ const sai_stat_id_t *counter_ids,
                                               _Out_ uint64_t           *counters)
{
    return mlnx_get_bridge_port_stats_ext(bridge_port_id,
                                          number_of_counters,
                                          counter_ids,
                                          SAI_STATS_MODE_READ,
                                          counters);
}

/**
 * @brief Clear bridge port statistics counters.
 *
 * @param[in] bridge_port_id Bridge port id
 * @param[in] number_of_counters Number of counters in the array
 * @param[in] counter_ids Specifies the array of counter ids
 *
 * @return #SAI_STATUS_SUCCESS on success, failure status code on error
 */
static sai_status_t mlnx_clear_bridge_port_stats(_In_ sai_object_id_t      bridge_port_id,
                                                 _In_ uint32_t             number_of_counters,
                                                 _In_ const sai_stat_id_t *counter_ids)
{
    return SAI_STATUS_NOT_IMPLEMENTED;
}

const sai_bridge_api_t mlnx_bridge_api = {
    mlnx_create_bridge,
    mlnx_remove_bridge,
    mlnx_set_bridge_attribute,
    mlnx_get_bridge_attribute,
    mlnx_get_bridge_stats,
    mlnx_get_bridge_stats_ext,
    mlnx_clear_bridge_stats,
    mlnx_create_bridge_port,
    mlnx_remove_bridge_port,
    mlnx_set_bridge_port_attribute,
    mlnx_get_bridge_port_attribute,
    mlnx_get_bridge_port_stats,
    mlnx_get_bridge_port_stats_ext,
    mlnx_clear_bridge_port_stats
};
