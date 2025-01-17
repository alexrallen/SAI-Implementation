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

#include "mlnx_sai.h"
#include <sx/utils/dbg_utils.h>
#include "assert.h"

static void SAI_dump_tunnel_getdb(_Out_ mlnx_tunnel_map_entry_t *tunnel_map_entry_db,
                                  _Out_ mlnx_tunnel_map_t       *tunnel_map_db,
                                  _Out_ mlnx_tunnel_entry_t     *tunnel_entry_db,
                                  _Out_ mlnx_tunneltable_t      *tunneltable_db,
                                  _Out_ mlnx_bmtor_bridge_t     *bmtor_bridge_db,
                                  _Out_ sx_bridge_id_t          *sx_bridge_id)
{
    assert(NULL != tunnel_map_entry_db);
    assert(NULL != tunnel_map_db);
    assert(NULL != tunnel_entry_db);
    assert(NULL != tunneltable_db);
    assert(NULL != bmtor_bridge_db);
    assert(NULL != sx_bridge_id);
    assert(NULL != g_sai_db_ptr);

    sai_db_read_lock();

    memcpy(tunnel_map_entry_db,
           g_sai_tunnel_db_ptr->tunnel_map_entry_db,
           MLNX_TUNNEL_MAP_ENTRY_MAX * sizeof(mlnx_tunnel_map_entry_t));

    memcpy(tunnel_map_db,
           g_sai_tunnel_db_ptr->tunnel_map_db,
           MLNX_TUNNEL_MAP_MAX * sizeof(mlnx_tunnel_map_t));

    memcpy(tunnel_entry_db,
           g_sai_tunnel_db_ptr->tunnel_entry_db,
           MAX_TUNNEL_DB_SIZE * sizeof(mlnx_tunnel_entry_t));

    memcpy(tunneltable_db,
           g_sai_tunnel_db_ptr->tunneltable_db,
           MLNX_TUNNELTABLE_SIZE * sizeof(mlnx_tunneltable_t));

    memcpy(bmtor_bridge_db,
           g_sai_tunnel_db_ptr->bmtor_bridge_db,
           MLNX_BMTOR_BRIDGE_MAX * sizeof(mlnx_bmtor_bridge_t));

    *sx_bridge_id = g_sai_db_ptr->sx_bridge_id;

    sai_db_unlock();
}

static void SAI_dump_tunnel_map_type_enum_to_str(_In_ sai_tunnel_map_type_t type, _Out_ char *str)
{
    assert(NULL != str);

    switch (type) {
    case SAI_TUNNEL_MAP_TYPE_OECN_TO_UECN:
        strcpy(str, "oecn2uecn");
        break;

    case SAI_TUNNEL_MAP_TYPE_UECN_OECN_TO_OECN:
        strcpy(str, "uoecn2oecn");
        break;

    case SAI_TUNNEL_MAP_TYPE_VNI_TO_VLAN_ID:
        strcpy(str, "vni2vlan");
        break;

    case SAI_TUNNEL_MAP_TYPE_VLAN_ID_TO_VNI:
        strcpy(str, "vlan2vni");
        break;

    case SAI_TUNNEL_MAP_TYPE_VNI_TO_BRIDGE_IF:
        strcpy(str, "vni2bridgeif");
        break;

    case SAI_TUNNEL_MAP_TYPE_BRIDGE_IF_TO_VNI:
        strcpy(str, "bridgeif2vni");
        break;

    case SAI_TUNNEL_MAP_TYPE_VNI_TO_VIRTUAL_ROUTER_ID:
        strcpy(str, "vni2vr");
        break;

    case SAI_TUNNEL_MAP_TYPE_VIRTUAL_ROUTER_ID_TO_VNI:
        strcpy(str, "vr2vni");
        break;

    default:
        strcpy(str, "unknown");
        break;
    }
}

static void SAI_dump_tunnel_map_entry_print(_In_ FILE *file, _In_ mlnx_tunnel_map_entry_t *mlnx_tunnel_map_entry)
{
    uint32_t                     ii = 0;
    uint32_t                     jj = 0;
    sai_object_id_t              obj_id = SAI_NULL_OBJECT_ID;
    mlnx_tunnel_map_entry_t      curr_mlnx_tunnel_map_entry;
    tunnel_map_entry_pair_info_t curr_pair_info;
    char                         type_str[LINE_LENGTH];
    dbg_utils_table_columns_t    tunnelmapentry_clmns[] = {
        {"sai oid",                   16, PARAM_UINT64_E, &obj_id},
        {"db idx",                    7,  PARAM_UINT32_E, &ii},
        {"type",                      12, PARAM_STRING_E, &type_str},
        {"map id",                    12, PARAM_UINT64_E, &curr_mlnx_tunnel_map_entry.tunnel_map_id},
        {"oecn key",                  12, PARAM_UINT8_E,  &curr_mlnx_tunnel_map_entry.oecn_key},
        {"oecn value",                12, PARAM_UINT8_E,  &curr_mlnx_tunnel_map_entry.oecn_value},
        {"uecn key",                  12, PARAM_UINT8_E,  &curr_mlnx_tunnel_map_entry.uecn_key},
        {"uecn value",                12, PARAM_UINT8_E,  &curr_mlnx_tunnel_map_entry.uecn_value},
        {"vlan key",                  12, PARAM_UINT16_E, &curr_mlnx_tunnel_map_entry.vlan_id_key},
        {"vlan value",                12, PARAM_UINT16_E, &curr_mlnx_tunnel_map_entry.vlan_id_value},
        {"vni key",                   12, PARAM_UINT32_E, &curr_mlnx_tunnel_map_entry.vni_id_key},
        {"vni value",                 12, PARAM_UINT32_E, &curr_mlnx_tunnel_map_entry.vni_id_value},
        {"bridge if key",             12, PARAM_UINT32_E, &curr_mlnx_tunnel_map_entry.bridge_id_key},
        {"bridge if value",           12, PARAM_UINT32_E, &curr_mlnx_tunnel_map_entry.bridge_id_value},
        {"vr id key",                 12, PARAM_UINT64_E, &curr_mlnx_tunnel_map_entry.vr_id_key},
        {"vr id value",               12, PARAM_UINT64_E, &curr_mlnx_tunnel_map_entry.vr_id_value},
        {"prev tunnel map entry idx", 12, PARAM_UINT32_E, &curr_mlnx_tunnel_map_entry.prev_tunnel_map_entry_idx},
        {"next tunnel map entry idx", 12, PARAM_UINT32_E, &curr_mlnx_tunnel_map_entry.next_tunnel_map_entry_idx},
        {NULL,                        0,  0,              NULL}
    };
    dbg_utils_table_columns_t    tunnelmapentry_pair_clmns[] = {
        {"vxlan db idx",        16, PARAM_UINT32_E, &jj},
        {"already bound",       16, PARAM_UINT8_E,  &curr_pair_info.pair_already_bound_to_tunnel},
        {"pair db idx",         16, PARAM_UINT32_E, &curr_pair_info.pair_tunnel_map_entry_idx},
        {"bmtor bridge db idx", 20, PARAM_UINT32_E, &curr_pair_info.bmtor_bridge_db_idx},
        {NULL,            0,  0,              NULL}
    };

    assert(NULL != mlnx_tunnel_map_entry);

    dbg_utils_print_general_header(file, "Tunnel map entry");

    dbg_utils_print_secondary_header(file, "mlnx_tunnel_map_entry");

    dbg_utils_print_table_headline(file, tunnelmapentry_clmns);

    for (ii = 0; ii < MLNX_TUNNEL_MAP_ENTRY_MAX; ii++) {
        if (mlnx_tunnel_map_entry[ii].in_use) {
            memcpy(&curr_mlnx_tunnel_map_entry, &mlnx_tunnel_map_entry[ii], sizeof(mlnx_tunnel_map_entry_t));

            if (SAI_STATUS_SUCCESS !=
                mlnx_create_object(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY, ii, NULL, &obj_id)) {
                obj_id = SAI_NULL_OBJECT_ID;
            }
            SAI_dump_tunnel_map_type_enum_to_str(mlnx_tunnel_map_entry[ii].tunnel_map_type,
                                                 type_str);
            dbg_utils_print_table_data_line(file, tunnelmapentry_clmns);

            dbg_utils_print_secondary_header(file, "tunnel map entry pair info");
            dbg_utils_print_table_headline(file, tunnelmapentry_pair_clmns);

            for (jj = 0; jj < MAX_VXLAN_TUNNEL; jj++) {
                if (mlnx_tunnel_map_entry[ii].pair_per_vxlan_array[jj].pair_exist) {
                    memcpy(&curr_pair_info, &mlnx_tunnel_map_entry[ii].pair_per_vxlan_array[jj],
                           sizeof(curr_pair_info));
                    dbg_utils_print_table_data_line(file, tunnelmapentry_pair_clmns);
                }
            }
        }
    }
}

static void SAI_dump_tunnel_map_print(_In_ FILE                    *file,
                                      _In_ mlnx_tunnel_map_t       *mlnx_tunnel_map,
                                      _In_ mlnx_tunnel_map_entry_t *mlnx_tunnel_map_entry)
{
    uint32_t                  ii = 0, jj = 0;
    sai_object_id_t           obj_id = SAI_NULL_OBJECT_ID;
    mlnx_tunnel_map_t         curr_mlnx_tunnel_map;
    mlnx_tunnel_map_entry_t   curr_mlnx_tunnel_map_entry;
    uint32_t                  curr_tunnel_idx = 0;
    uint32_t                  tunnel_map_entry_idx = MLNX_TUNNEL_MAP_ENTRY_INVALID;
    char                      type_str[LINE_LENGTH];
    dbg_utils_table_columns_t tunnelmap_clmns[] = {
        {"sai oid",                   16, PARAM_UINT64_E, &obj_id},
        {"db idx",                    7,  PARAM_UINT32_E, &ii},
        {"type",                      12, PARAM_STRING_E, &type_str},
        {"tunnel cnt",                10, PARAM_UINT32_E, &curr_mlnx_tunnel_map.tunnel_cnt},
        {"tunnel map entry cnt",      20, PARAM_UINT32_E, &curr_mlnx_tunnel_map.tunnel_map_entry_cnt},
        {"tunnel map entry head idx", 25, PARAM_UINT32_E, &curr_mlnx_tunnel_map.tunnel_map_entry_head_idx},
        {"tunnel map entry tail idx", 25, PARAM_UINT32_E, &curr_mlnx_tunnel_map.tunnel_map_entry_tail_idx},
        {NULL,                        0,  0,              NULL}
    };
    dbg_utils_table_columns_t sai_tunnelmap_oecn2uecn_clmns[] = {
        {"db idx",               7,  PARAM_UINT32_E, &ii},
        {"tunnel map entry idx", 20, PARAM_UINT32_E, &tunnel_map_entry_idx},
        {"key oecn",             8,  PARAM_UINT8_E,  &curr_mlnx_tunnel_map_entry.oecn_key},
        {"val uecn",             8,  PARAM_UINT8_E,  &curr_mlnx_tunnel_map_entry.uecn_value},
        {NULL,                   0,  0,              NULL}
    };
    dbg_utils_table_columns_t sai_tunnelmap_uecnoecn2oecn_clmns[] = {
        {"db idx",               7,  PARAM_UINT32_E, &ii},
        {"tunnel map entry idx", 20, PARAM_UINT32_E, &tunnel_map_entry_idx},
        {"key oecn",             8,  PARAM_UINT8_E,  &curr_mlnx_tunnel_map_entry.oecn_key},
        {"key uecn",             8,  PARAM_UINT8_E,  &curr_mlnx_tunnel_map_entry.uecn_key},
        {"val oecn",             8,  PARAM_UINT8_E,  &curr_mlnx_tunnel_map_entry.oecn_value},
        {NULL,                   0,  0,              NULL}
    };
    dbg_utils_table_columns_t sai_tunnelmap_vni2vlan_clmns[] = {
        {"db idx",               7,  PARAM_UINT32_E, &ii},
        {"tunnel map entry idx", 20, PARAM_UINT32_E, &tunnel_map_entry_idx},
        {"key vni",              11, PARAM_UINT32_E, &curr_mlnx_tunnel_map_entry.vni_id_key},
        {"val vlan",             8,  PARAM_UINT16_E, &curr_mlnx_tunnel_map_entry.vlan_id_value},
        {NULL,                   0,  0,              NULL}
    };
    dbg_utils_table_columns_t sai_tunnelmap_vlan2vni_clmns[] = {
        {"db idx",               7,  PARAM_UINT32_E, &ii},
        {"tunnel map entry idx", 20, PARAM_UINT32_E, &tunnel_map_entry_idx},
        {"key vlan",             8,  PARAM_UINT16_E, &curr_mlnx_tunnel_map_entry.vlan_id_key},
        {"val vni",              11, PARAM_UINT32_E, &curr_mlnx_tunnel_map_entry.vni_id_value},
        {NULL,                   0,  0,              NULL}
    };
    dbg_utils_table_columns_t sai_tunnelmap_vni2bridgeif_clmns[] = {
        {"db idx",               7,  PARAM_UINT32_E, &ii},
        {"tunnel map entry idx", 20, PARAM_UINT32_E, &tunnel_map_entry_idx},
        {"key vni",              11, PARAM_UINT32_E, &curr_mlnx_tunnel_map_entry.vni_id_key},
        {"val bridge if",        13, PARAM_UINT64_E, &curr_mlnx_tunnel_map_entry.bridge_id_value},
        {NULL,                   0,  0,              NULL}
    };
    dbg_utils_table_columns_t sai_tunnelmap_bridgeif2vni_clmns[] = {
        {"db idx",               7,  PARAM_UINT32_E, &ii},
        {"tunnel map entry idx", 20, PARAM_UINT32_E, &tunnel_map_entry_idx},
        {"key bridge if",        8,  PARAM_UINT64_E, &curr_mlnx_tunnel_map_entry.bridge_id_key},
        {"val vni",              11, PARAM_UINT32_E, &curr_mlnx_tunnel_map_entry.vni_id_value},
        {NULL,                   0,  0,              NULL}
    };
    dbg_utils_table_columns_t sai_tunnelmap_vni2vr_clmns[] = {
        {"db idx",               7,  PARAM_UINT32_E, &ii},
        {"tunnel map entry idx", 20, PARAM_UINT32_E, &tunnel_map_entry_idx},
        {"key vni",              11, PARAM_UINT32_E, &curr_mlnx_tunnel_map_entry.vni_id_key},
        {"val vr id",            13, PARAM_UINT64_E, &curr_mlnx_tunnel_map_entry.vr_id_value},
        {NULL,                   0,  0,              NULL}
    };
    dbg_utils_table_columns_t sai_tunnelmap_vr2vni_clmns[] = {
        {"db idx",               7,  PARAM_UINT32_E, &ii},
        {"tunnel map entry idx", 20, PARAM_UINT32_E, &tunnel_map_entry_idx},
        {"key vr id",            11, PARAM_UINT64_E, &curr_mlnx_tunnel_map_entry.vr_id_key},
        {"val vni",              13, PARAM_UINT32_E, &curr_mlnx_tunnel_map_entry.vni_id_value},
        {NULL,                   0,  0,              NULL}
    };
    dbg_utils_table_columns_t sai_tunnelmap_tunnel_idx_clmns[] = {
        {"db idx",           7,  PARAM_UINT32_E, &ii},
        {"tunnel array idx", 17, PARAM_UINT32_E, &jj},
        {"tunnel idx",       11, PARAM_UINT32_E, &curr_tunnel_idx},
        {NULL,               0,  0,              NULL}
    };

    assert(NULL != mlnx_tunnel_map);

    dbg_utils_print_general_header(file, "Tunnel map");

    dbg_utils_print_secondary_header(file, "mlnx_tunnel_map");

    dbg_utils_print_table_headline(file, tunnelmap_clmns);

    for (ii = 0; ii < MLNX_TUNNEL_MAP_MAX; ii++) {
        if (mlnx_tunnel_map[ii].in_use) {
            memcpy(&curr_mlnx_tunnel_map, &mlnx_tunnel_map[ii], sizeof(mlnx_tunnel_map_t));

            if (SAI_STATUS_SUCCESS !=
                mlnx_create_object(SAI_OBJECT_TYPE_TUNNEL_MAP, ii, NULL, &obj_id)) {
                obj_id = SAI_NULL_OBJECT_ID;
            }
            SAI_dump_tunnel_map_type_enum_to_str(mlnx_tunnel_map[ii].tunnel_map_type,
                                                 type_str);
            dbg_utils_print_table_data_line(file, tunnelmap_clmns);
        }
    }

    dbg_utils_print_secondary_header(file, "sai_tunnel_map");

    for (ii = 0; ii < MLNX_TUNNEL_MAP_MAX; ii++) {
        if (mlnx_tunnel_map[ii].in_use) {
            dbg_utils_print_table_headline(file, sai_tunnelmap_tunnel_idx_clmns);
            for (jj = 0; jj < mlnx_tunnel_map[ii].tunnel_cnt; jj++) {
                curr_tunnel_idx = mlnx_tunnel_map[ii].tunnel_idx[jj];
                dbg_utils_print_table_data_line(file, sai_tunnelmap_tunnel_idx_clmns);
            }

            switch (mlnx_tunnel_map[ii].tunnel_map_type) {
            case SAI_TUNNEL_MAP_TYPE_OECN_TO_UECN:
                dbg_utils_print_table_headline(file, sai_tunnelmap_oecn2uecn_clmns);
                break;

            case SAI_TUNNEL_MAP_TYPE_UECN_OECN_TO_OECN:
                dbg_utils_print_table_headline(file, sai_tunnelmap_uecnoecn2oecn_clmns);
                break;

            case SAI_TUNNEL_MAP_TYPE_VNI_TO_VLAN_ID:
                dbg_utils_print_table_headline(file, sai_tunnelmap_vni2vlan_clmns);
                break;

            case SAI_TUNNEL_MAP_TYPE_VLAN_ID_TO_VNI:
                dbg_utils_print_table_headline(file, sai_tunnelmap_vlan2vni_clmns);
                break;

            case SAI_TUNNEL_MAP_TYPE_VNI_TO_BRIDGE_IF:
                dbg_utils_print_table_headline(file, sai_tunnelmap_vni2bridgeif_clmns);
                break;

            case SAI_TUNNEL_MAP_TYPE_BRIDGE_IF_TO_VNI:
                dbg_utils_print_table_headline(file, sai_tunnelmap_bridgeif2vni_clmns);
                break;

            case SAI_TUNNEL_MAP_TYPE_VNI_TO_VIRTUAL_ROUTER_ID:
                dbg_utils_print_table_headline(file, sai_tunnelmap_vni2vr_clmns);
                break;

            case SAI_TUNNEL_MAP_TYPE_VIRTUAL_ROUTER_ID_TO_VNI:
                dbg_utils_print_table_headline(file, sai_tunnelmap_vr2vni_clmns);
                break;

            default:
                break;
            }

            for (jj = mlnx_tunnel_map[ii].tunnel_map_entry_head_idx;
                 jj != MLNX_TUNNEL_MAP_ENTRY_INVALID;
                 jj = mlnx_tunnel_map_entry[jj].next_tunnel_map_entry_idx) {
                tunnel_map_entry_idx = jj;

                switch (mlnx_tunnel_map[ii].tunnel_map_type) {
                case SAI_TUNNEL_MAP_TYPE_OECN_TO_UECN:
                    curr_mlnx_tunnel_map_entry.oecn_key = mlnx_tunnel_map_entry[jj].oecn_key;
                    curr_mlnx_tunnel_map_entry.uecn_value = mlnx_tunnel_map_entry[jj].uecn_value;
                    dbg_utils_print_table_data_line(file, sai_tunnelmap_oecn2uecn_clmns);
                    break;

                case SAI_TUNNEL_MAP_TYPE_UECN_OECN_TO_OECN:
                    curr_mlnx_tunnel_map_entry.oecn_key = mlnx_tunnel_map_entry[jj].oecn_key;
                    curr_mlnx_tunnel_map_entry.uecn_key = mlnx_tunnel_map_entry[jj].uecn_key;
                    curr_mlnx_tunnel_map_entry.oecn_value = mlnx_tunnel_map_entry[jj].oecn_value;
                    dbg_utils_print_table_data_line(file, sai_tunnelmap_uecnoecn2oecn_clmns);
                    break;

                case SAI_TUNNEL_MAP_TYPE_VNI_TO_VLAN_ID:
                    curr_mlnx_tunnel_map_entry.vni_id_key = mlnx_tunnel_map_entry[jj].vni_id_key;
                    curr_mlnx_tunnel_map_entry.vlan_id_value = mlnx_tunnel_map_entry[jj].vlan_id_value;
                    dbg_utils_print_table_data_line(file, sai_tunnelmap_vni2vlan_clmns);
                    break;

                case SAI_TUNNEL_MAP_TYPE_VLAN_ID_TO_VNI:
                    curr_mlnx_tunnel_map_entry.vlan_id_key = mlnx_tunnel_map_entry[jj].vlan_id_key;
                    curr_mlnx_tunnel_map_entry.vni_id_value = mlnx_tunnel_map_entry[jj].vni_id_value;
                    dbg_utils_print_table_data_line(file, sai_tunnelmap_vlan2vni_clmns);
                    break;

                case SAI_TUNNEL_MAP_TYPE_VNI_TO_BRIDGE_IF:
                    curr_mlnx_tunnel_map_entry.vni_id_key = mlnx_tunnel_map_entry[jj].vni_id_key;
                    curr_mlnx_tunnel_map_entry.bridge_id_value = mlnx_tunnel_map_entry[jj].bridge_id_value;
                    dbg_utils_print_table_data_line(file, sai_tunnelmap_vni2bridgeif_clmns);
                    break;

                case SAI_TUNNEL_MAP_TYPE_BRIDGE_IF_TO_VNI:
                    curr_mlnx_tunnel_map_entry.bridge_id_key = mlnx_tunnel_map_entry[jj].bridge_id_key;
                    curr_mlnx_tunnel_map_entry.vni_id_value = mlnx_tunnel_map_entry[jj].vni_id_value;
                    dbg_utils_print_table_data_line(file, sai_tunnelmap_bridgeif2vni_clmns);
                    break;

                case SAI_TUNNEL_MAP_TYPE_VNI_TO_VIRTUAL_ROUTER_ID:
                    curr_mlnx_tunnel_map_entry.vni_id_key = mlnx_tunnel_map_entry[jj].vni_id_key;
                    curr_mlnx_tunnel_map_entry.vr_id_value = mlnx_tunnel_map_entry[jj].vr_id_value;
                    dbg_utils_print_table_data_line(file, sai_tunnelmap_vni2vr_clmns);
                    break;

                case SAI_TUNNEL_MAP_TYPE_VIRTUAL_ROUTER_ID_TO_VNI:
                    curr_mlnx_tunnel_map_entry.vr_id_key = mlnx_tunnel_map_entry[jj].vr_id_key;
                    curr_mlnx_tunnel_map_entry.vni_id_value = mlnx_tunnel_map_entry[jj].vni_id_value;
                    dbg_utils_print_table_data_line(file, sai_tunnelmap_vr2vni_clmns);
                    break;

                default:
                    break;
                }
            }
        }
    }
}

static void SAI_dump_tunnel_sai_tunnel_type_enum_to_str(_In_ sai_tunnel_type_t type, _Out_ char *str)
{
    assert(NULL != str);

    switch (type) {
    case SAI_TUNNEL_TYPE_IPINIP:
        strcpy(str, "IPinIP");
        break;

    case SAI_TUNNEL_TYPE_IPINIP_GRE:
        strcpy(str, "IPinIP GRE");
        break;

    case SAI_TUNNEL_TYPE_VXLAN:
        strcpy(str, "VXLAN");
        break;

    case SAI_TUNNEL_TYPE_MPLS:
        strcpy(str, "MPLS");
        break;

    default:
        strcpy(str, "unknown");
        break;
    }
}

static void SAI_dump_tunnel_print(_In_ FILE *file, _In_ mlnx_tunnel_entry_t *tunnel_db)
{
    uint32_t                  ii = 0, jj = 0;
    sai_object_id_t           obj_id = SAI_NULL_OBJECT_ID;
    mlnx_tunnel_entry_t       curr_tunnel_db;
    char                      sai_tunnel_type_str[LINE_LENGTH];
    dbg_utils_table_columns_t tunnel_clmns[] = {
        {"sai oid",            16, PARAM_UINT64_E, &obj_id},
        {"db idx",             7,  PARAM_UINT32_E, &ii},
        {"SAI tunnel type",    15, PARAM_STRING_E, &sai_tunnel_type_str},
        {"sx tunnel id ipv4",  17, PARAM_UINT32_E, &curr_tunnel_db.sx_tunnel_id_ipv4},
        {"sx tunnel id ipv6",  17, PARAM_UINT32_E, &curr_tunnel_db.sx_tunnel_id_ipv6},
        {"ipv4 created",       12, PARAM_UINT8_E,  &curr_tunnel_db.ipv4_created},
        {"ipv6 created",       12, PARAM_UINT8_E,  &curr_tunnel_db.ipv6_created},
        {"sx o rif ipv6",      16, PARAM_UINT16_E, &curr_tunnel_db.sx_overlay_rif_ipv6},
        {"vxlan u rif",        16, PARAM_UINT64_E, &curr_tunnel_db.sai_underlay_rif},
        {"encap map cnt",      13, PARAM_UINT32_E, &curr_tunnel_db.sai_tunnel_map_encap_cnt},
        {"decap map cnt",      13, PARAM_UINT32_E, &curr_tunnel_db.sai_tunnel_map_decap_cnt},
        {"term table cnt",     14, PARAM_UINT32_E, &curr_tunnel_db.term_table_cnt},
        {NULL,                 0,  0,              NULL}
    };
    dbg_utils_table_columns_t tunnel_encap_map_clmns[] = {
        {"db idx",       7,  PARAM_UINT32_E, &ii},
        {"encap map id", 16, PARAM_UINT64_E, &obj_id},
        {NULL,           0,  0,              NULL}
    };
    dbg_utils_table_columns_t tunnel_decap_map_clmns[] = {
        {"db idx",       7,  PARAM_UINT32_E, &ii},
        {"decap map id", 16, PARAM_UINT64_E, &obj_id},
        {NULL,           0,  0,              NULL}
    };

    assert(NULL != tunnel_db);

    dbg_utils_print_general_header(file, "Tunnel");

    dbg_utils_print_secondary_header(file, "tunnel_db");

    dbg_utils_print_table_headline(file, tunnel_clmns);

    for (ii = 0; ii < MAX_TUNNEL_DB_SIZE; ii++) {
        if (tunnel_db[ii].is_used) {
            memcpy(&curr_tunnel_db, &tunnel_db[ii], sizeof(mlnx_tunnel_entry_t));
            if (SAI_STATUS_SUCCESS !=
                mlnx_create_object(SAI_OBJECT_TYPE_TUNNEL, ii, NULL, &obj_id)) {
                obj_id = SAI_NULL_OBJECT_ID;
            }

            SAI_dump_tunnel_sai_tunnel_type_enum_to_str(curr_tunnel_db.sai_tunnel_type, sai_tunnel_type_str);
            dbg_utils_print_table_data_line(file, tunnel_clmns);
        }
    }

    dbg_utils_print_secondary_header(file, "tunnel encap map");

    dbg_utils_print_table_headline(file, tunnel_encap_map_clmns);

    for (ii = 0; ii < MAX_TUNNEL_DB_SIZE; ii++) {
        if (tunnel_db[ii].is_used) {
            for (jj = 0; jj < tunnel_db[ii].sai_tunnel_map_encap_cnt; jj++) {
                obj_id = tunnel_db[ii].sai_tunnel_map_encap_id_array[jj];
                dbg_utils_print_table_data_line(file, tunnel_encap_map_clmns);
            }
        }
    }

    dbg_utils_print_secondary_header(file, "tunnel decap map");

    dbg_utils_print_table_headline(file, tunnel_decap_map_clmns);

    for (ii = 0; ii < MAX_TUNNEL_DB_SIZE; ii++) {
        if (tunnel_db[ii].is_used) {
            for (jj = 0; jj < tunnel_db[ii].sai_tunnel_map_decap_cnt; jj++) {
                obj_id = tunnel_db[ii].sai_tunnel_map_decap_id_array[jj];
                dbg_utils_print_table_data_line(file, tunnel_decap_map_clmns);
            }
        }
    }
}

static void SAI_dump_sdk_tunnel_type_enum_to_str(_In_ sx_tunnel_type_e type, _Out_ char *str)
{
    assert(NULL != str);

    switch (type) {
    case SX_TUNNEL_TYPE_IPINIP_P2P_IPV4_IN_IPV4:
        strcpy(str, "ipinip");
        break;

    case SX_TUNNEL_TYPE_IPINIP_P2P_IPV4_IN_GRE:
        strcpy(str, "ipinip gre");
        break;

    case SX_TUNNEL_TYPE_NVE_VXLAN:
        strcpy(str, "vxlan");
        break;

    case SX_TUNNEL_TYPE_NVE_VXLAN_GPE:
        strcpy(str, "vxlan gpe");
        break;

    case SX_TUNNEL_TYPE_NVE_GENEVE:
        strcpy(str, "geneve");
        break;

    case SX_TUNNEL_TYPE_NVE_NVGRE:
        strcpy(str, "nvgre");
        break;

    default:
        strcpy(str, "unknown");
        break;
    }
}

static void SAI_dump_sdk_tunnel_table_type_enum_to_str(_In_ sx_tunnel_decap_key_fields_type_e type, _Out_ char *str)
{
    assert(NULL != str);

    switch (type) {
    case SX_TUNNEL_DECAP_KEY_FIELDS_TYPE_DIP:
        strcpy(str, "dip");
        break;

    case SX_TUNNEL_DECAP_KEY_FIELDS_TYPE_DIP_SIP:
        strcpy(str, "dip sip");
        break;

    default:
        strcpy(str, "unknown");
        break;
    }
}

static void SAI_dump_tunnel_table_print(_In_ FILE *file, _In_ mlnx_tunneltable_t *mlnx_tunneltable)
{
    uint32_t                  ii = 0;
    sai_object_id_t           obj_id = SAI_NULL_OBJECT_ID;
    mlnx_tunneltable_t        curr_mlnx_tunneltable;
    char                      tunnel_type_str[LINE_LENGTH];
    char                      field_type_str[LINE_LENGTH];
    dbg_utils_table_columns_t tunneltable_clmns[] = {
        {"sai obj id",          11, PARAM_UINT64_E, &obj_id},
        {"db idx",              8,  PARAM_UINT32_E, &ii},
        {"tunnel type",         11, PARAM_STRING_E, &tunnel_type_str},
        {"tunnel db idx",       13, PARAM_UINT32_E, &curr_mlnx_tunneltable.tunnel_db_idx},
        {"tunnel lazy created", 19, PARAM_UINT8_E,  &curr_mlnx_tunneltable.tunnel_lazy_created},
        {"field type",          10, PARAM_STRING_E, &field_type_str},
        {"u vrid",              10, PARAM_STRING_E, &curr_mlnx_tunneltable.sdk_tunnel_decap_key_ipv4.underlay_vrid},
        {"u dipv4",             15, PARAM_IPV4_E,
         &curr_mlnx_tunneltable.sdk_tunnel_decap_key_ipv4.underlay_dip.addr.ipv4},
        {"u dipv6",             39, PARAM_IPV6_E,
         &curr_mlnx_tunneltable.sdk_tunnel_decap_key_ipv4.underlay_dip.addr.ipv6},
        {"u sipv4",             15, PARAM_IPV4_E,
         &curr_mlnx_tunneltable.sdk_tunnel_decap_key_ipv4.underlay_sip.addr.ipv4},
        {"u sipv6",             39, PARAM_IPV6_E,
         &curr_mlnx_tunneltable.sdk_tunnel_decap_key_ipv4.underlay_sip.addr.ipv6},
        {NULL,                  0,  0,              NULL}
    };

    assert(NULL != mlnx_tunneltable);

    dbg_utils_print_general_header(file, "Tunnel table");

    dbg_utils_print_secondary_header(file, "mlnx_tunneltable");

    dbg_utils_print_table_headline(file, tunneltable_clmns);

    for (ii = 0; ii < MLNX_TUNNELTABLE_SIZE; ii++) {
        if (mlnx_tunneltable[ii].in_use) {
            memcpy(&curr_mlnx_tunneltable, &mlnx_tunneltable[ii], sizeof(mlnx_tunneltable_t));

            if (SAI_STATUS_SUCCESS !=
                mlnx_create_object(SAI_OBJECT_TYPE_TUNNEL_TERM_TABLE_ENTRY, ii, NULL, &obj_id)) {
                obj_id = SAI_NULL_OBJECT_ID;
            }
            SAI_dump_sdk_tunnel_type_enum_to_str(mlnx_tunneltable[ii].sdk_tunnel_decap_key_ipv4.tunnel_type,
                                                 tunnel_type_str);
            SAI_dump_sdk_tunnel_table_type_enum_to_str(mlnx_tunneltable[ii].sdk_tunnel_decap_key_ipv4.type,
                                                       field_type_str);

            dbg_utils_print_table_data_line(file, tunneltable_clmns);
        }
    }
}

static void SAI_dump_bmtor_bridge_print(_In_ FILE *file, _In_ mlnx_bmtor_bridge_t *mlnx_bmtor_bridge)
{
    uint32_t                  ii = 0;
    mlnx_bmtor_bridge_t       curr_mlnx_bmtor_bridge;
    dbg_utils_table_columns_t bmtor_bridge_clmns[] = {
        {"db idx",               8,  PARAM_UINT32_E, &ii},
        {"is default",           11, PARAM_UINT8_E,  &curr_mlnx_bmtor_bridge.is_default},
        {"vrf oid",              11, PARAM_UINT64_E, &curr_mlnx_bmtor_bridge.connected_vrf_oid},
        {"bridge oid",           11, PARAM_UINT64_E, &curr_mlnx_bmtor_bridge.bridge_oid},
        {"rif oid",              11, PARAM_UINT64_E, &curr_mlnx_bmtor_bridge.rif_oid},
        {"bridge bport oid",     16, PARAM_UINT64_E, &curr_mlnx_bmtor_bridge.bridge_bport_oid},
        {"tunnel bport oid",     16, PARAM_UINT64_E, &curr_mlnx_bmtor_bridge.tunnel_bport_oid},
        {"sx vxlan tunnel id",   18, PARAM_UINT32_E, &curr_mlnx_bmtor_bridge.sx_vxlan_tunnel_id},
        {"vni",                  8,  PARAM_UINT32_E, &curr_mlnx_bmtor_bridge.vni},
        {"counter",              9,  PARAM_UINT32_E, &curr_mlnx_bmtor_bridge.counter},
        {NULL,                   0,  0,              NULL}
    };

    assert(NULL != mlnx_bmtor_bridge);

    dbg_utils_print_general_header(file, "BMTOR bridge");

    dbg_utils_print_secondary_header(file, "mlnx_bmtor_bridge");

    dbg_utils_print_table_headline(file, bmtor_bridge_clmns);

    for (ii = 0; ii < MLNX_BMTOR_BRIDGE_MAX; ii++) {
        if (mlnx_bmtor_bridge[ii].in_use) {
            memcpy(&curr_mlnx_bmtor_bridge, &mlnx_bmtor_bridge[ii], sizeof(mlnx_bmtor_bridge_t));

            dbg_utils_print_table_data_line(file, bmtor_bridge_clmns);
        }
    }
}

static void SAI_dump_bridge_print(_In_ FILE *file, _In_ sx_bridge_id_t *sx_bridge_id)
{
    assert(NULL != sx_bridge_id);

    dbg_utils_print_general_header(file, "Bridge");

    dbg_utils_print_field(file, "sx bridge id", sx_bridge_id, PARAM_UINT16_E);
    dbg_utils_print(file, "\n");
}

void SAI_dump_tunnel(_In_ FILE *file)
{
    mlnx_tunnel_map_entry_t *tunnel_map_entry_db = NULL;
    mlnx_tunnel_map_t       *tunnel_map_db = NULL;
    mlnx_tunnel_entry_t     *tunnel_entry_db = NULL;
    mlnx_tunneltable_t      *tunneltable_db = NULL;
    mlnx_bmtor_bridge_t     *bmtor_bridge_db = NULL;
    sx_bridge_id_t           sx_bridge_id = 0;

    tunnel_map_entry_db =
        (mlnx_tunnel_map_entry_t*)calloc(MLNX_TUNNEL_MAP_ENTRY_MAX, sizeof(mlnx_tunnel_map_entry_t));
    tunnel_map_db = (mlnx_tunnel_map_t*)calloc(MLNX_TUNNEL_MAP_MAX, sizeof(mlnx_tunnel_map_t));
    tunnel_entry_db = (mlnx_tunnel_entry_t*)calloc(MAX_TUNNEL_DB_SIZE, sizeof(mlnx_tunnel_entry_t));
    tunneltable_db = (mlnx_tunneltable_t*)calloc(MLNX_TUNNELTABLE_SIZE, sizeof(mlnx_tunneltable_t));
    bmtor_bridge_db = (mlnx_bmtor_bridge_t*)calloc(MLNX_BMTOR_BRIDGE_MAX, sizeof(mlnx_bmtor_bridge_t));
    if ((!tunnel_map_entry_db) || (!tunnel_map_db) || (!tunnel_entry_db) || (!tunneltable_db) || (!bmtor_bridge_db)) {
        if (tunnel_map_entry_db) {
            free(tunnel_map_entry_db);
        }
        if (tunnel_map_db) {
            free(tunnel_map_db);
        }
        if (tunnel_entry_db) {
            free(tunnel_entry_db);
        }
        if (tunneltable_db) {
            free(tunneltable_db);
        }
        if (bmtor_bridge_db) {
            free(bmtor_bridge_db);
        }
        return;
    }

    SAI_dump_tunnel_getdb(tunnel_map_entry_db,
                          tunnel_map_db,
                          tunnel_entry_db,
                          tunneltable_db,
                          bmtor_bridge_db,
                          &sx_bridge_id);
    dbg_utils_print_module_header(file, "SAI Tunnel");
    SAI_dump_tunnel_map_entry_print(file, tunnel_map_entry_db);
    SAI_dump_tunnel_map_print(file, tunnel_map_db, tunnel_map_entry_db);
    SAI_dump_tunnel_print(file, tunnel_entry_db);
    SAI_dump_tunnel_table_print(file, tunneltable_db);
    SAI_dump_bmtor_bridge_print(file, bmtor_bridge_db);
    SAI_dump_bridge_print(file, &sx_bridge_id);

    free(tunnel_map_entry_db);
    free(tunnel_map_db);
    free(tunnel_entry_db);
    free(tunneltable_db);
    free(bmtor_bridge_db);
}
