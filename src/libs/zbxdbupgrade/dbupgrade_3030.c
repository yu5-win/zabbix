/*
** Zabbix
** Copyright (C) 2001-2017 Zabbix SIA
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**/

#include "common.h"
#include "db.h"
#include "dbupgrade.h"

/*
 * 3.4 development database patches
 */

#ifndef HAVE_SQLITE3

static int	DBpatch_3030000(void)
{
	const ZBX_FIELD	field = {"ipmi_authtype", "-1", NULL, NULL, 0, ZBX_TYPE_INT, ZBX_NOTNULL, 0};

	return DBset_default("hosts", &field);
}

static int	DBpatch_3030001(void)
{
	const ZBX_FIELD field = {"snmp_oid", "", NULL, NULL, 512, ZBX_TYPE_CHAR, ZBX_NOTNULL, 0};

	return DBmodify_field_type("items", &field);
}

static int	DBpatch_3030002(void)
{
	const ZBX_FIELD field = {"key_", "", NULL, NULL, 512, ZBX_TYPE_CHAR, ZBX_NOTNULL, 0};

	return DBmodify_field_type("dchecks", &field);
}

static int	DBpatch_3030003(void)
{
	return DBdrop_field("proxy_dhistory", "type");
}

static int	DBpatch_3030004(void)
{
	return DBdrop_field("proxy_dhistory", "key_");
}

static int	DBpatch_3030005(void)
{
	return DBdrop_foreign_key("dservices", 2);
}

static int	DBpatch_3030006(void)
{
	return DBdrop_index("dservices", "dservices_1");
}

static int	DBpatch_3030007(void)
{
	DB_ROW			row;
	DB_RESULT		result;
	zbx_vector_uint64_t	dserviceids;
	zbx_uint64_t		dserviceid;
	int			ret = SUCCEED;

	zbx_vector_uint64_create(&dserviceids);

	/* After dropping fields type and key_ from table dservices there is no guarantee that a unique
	index with fields dcheckid, ip and port can be created. To create a unique index for the same
	fields later this will delete rows where all three of them are identical only leaving the latest. */
	result = DBselect(
			"select ds.dserviceid"
			" from dservices ds"
			" where not exists ("
				"select null"
				" from dchecks dc"
				" where ds.dcheckid = dc.dcheckid"
					" and ds.type = dc.type"
					" and ds.key_ = dc.key_"
			")");

	while (NULL != (row = DBfetch(result)))
	{
		ZBX_STR2UINT64(dserviceid, row[0]);

		zbx_vector_uint64_append(&dserviceids, dserviceid);
	}
	DBfree_result(result);

	zbx_vector_uint64_sort(&dserviceids, ZBX_DEFAULT_UINT64_COMPARE_FUNC);

	if (0 != dserviceids.values_num)
		ret = DBexecute_multiple_query("delete from dservices where", "dserviceid", &dserviceids);

	zbx_vector_uint64_destroy(&dserviceids);

	return ret;
}

static int	DBpatch_3030008(void)
{
	return DBdrop_field("dservices", "type");
}

static int	DBpatch_3030009(void)
{
	return DBdrop_field("dservices", "key_");
}

static int	DBpatch_3030010(void)
{
	return DBcreate_index("dservices", "dservices_1", "dcheckid,ip,port", 1);
}

static int	DBpatch_3030011(void)
{
	const ZBX_FIELD	field = {"dcheckid", NULL, "dchecks", "dcheckid", 0, 0, 0, ZBX_FK_CASCADE_DELETE};

	return DBadd_foreign_key("dservices", 2, &field);
}

static int	DBpatch_3030012(void)
{
	const ZBX_FIELD field = {"snmp_lastsize", "0", NULL, NULL, 0, ZBX_TYPE_UINT, ZBX_NOTNULL, 0};

	return DBmodify_field_type("globalvars", &field);
}

static int	DBpatch_3030013(void)
{
	const ZBX_FIELD field = {"period", "1-7,00:00-24:00", NULL, NULL, 1024, ZBX_TYPE_CHAR, ZBX_NOTNULL, 0};

	return DBmodify_field_type("media", &field);
}

static int	DBpatch_3030014(void)
{
	const ZBX_FIELD field = {"delay_flex", "", NULL, NULL, 1024, ZBX_TYPE_CHAR, ZBX_NOTNULL, 0};

	return DBmodify_field_type("items", &field);
}

static int	DBpatch_3030015(void)
{
	const ZBX_TABLE table =
			{"item_preproc", "item_preprocid", 0,
				{
					{"item_preprocid", NULL, NULL, NULL, 0, ZBX_TYPE_ID, ZBX_NOTNULL, 0},
					{"itemid", NULL, NULL, NULL, 0, ZBX_TYPE_ID, ZBX_NOTNULL, 0},
					{"step", "0", NULL, NULL, 0, ZBX_TYPE_INT, ZBX_NOTNULL, 0},
					{"type", "0", NULL, NULL, 0, ZBX_TYPE_INT, ZBX_NOTNULL, 0},
					{"params", "", NULL, NULL, 255, ZBX_TYPE_CHAR, ZBX_NOTNULL, 0},
					{0}
				},
				NULL
			};

	return DBcreate_table(&table);
}

static int	DBpatch_3030016(void)
{
	return DBcreate_index("item_preproc", "item_preproc_1", "itemid, step", 0);
}

static int	DBpatch_3030017(void)
{
	const ZBX_FIELD	field = {"itemid", NULL, "items", "itemid", 0, 0, 0, ZBX_FK_CASCADE_DELETE};

	return DBadd_foreign_key("item_preproc", 1, &field);
}

static void	DBpatch_3030018_add_numeric_preproc_steps(zbx_db_insert_t *db_insert, zbx_uint64_t itemid,
		unsigned char data_type, const char *formula, unsigned char delta)
{
	int	step = 1;

	switch (data_type)
	{
		case ITEM_DATA_TYPE_BOOLEAN:
			zbx_db_insert_add_values(db_insert, __UINT64_C(0), itemid, step++, ZBX_PREPROC_BOOL2DEC, "");
			break;
		case ITEM_DATA_TYPE_OCTAL:
			zbx_db_insert_add_values(db_insert, __UINT64_C(0), itemid, step++, ZBX_PREPROC_OCT2DEC, "");
			break;
		case ITEM_DATA_TYPE_HEXADECIMAL:
			zbx_db_insert_add_values(db_insert, __UINT64_C(0), itemid, step++, ZBX_PREPROC_HEX2DEC, "");
			break;
	}

	switch (delta)
	{
		case ITEM_STORE_SPEED_PER_SECOND:
			zbx_db_insert_add_values(db_insert, __UINT64_C(0), itemid, step++, ZBX_PREPROC_DELTA_SPEED, "");
			break;
		case ITEM_STORE_SIMPLE_CHANGE:
			zbx_db_insert_add_values(db_insert, __UINT64_C(0), itemid, step++, ZBX_PREPROC_DELTA_VALUE, "");
			break;
	}

	if (NULL != formula)
		zbx_db_insert_add_values(db_insert, __UINT64_C(0), itemid, step++, ZBX_PREPROC_MULTIPLIER, formula);

}

static int	DBpatch_3030018(void)
{
	DB_ROW		row;
	DB_RESULT	result;
	unsigned char	value_type, data_type, delta;
	zbx_db_insert_t	db_insert;
	zbx_uint64_t	itemid;
	const char	*formula;
	int		ret;

	zbx_db_insert_prepare(&db_insert, "item_preproc", "item_preprocid", "itemid", "step", "type", "params", NULL);

	result = DBselect("select itemid,value_type,data_type,multiplier,formula,delta from items");

	while (NULL != (row = DBfetch(result)))
	{
		ZBX_STR2UINT64(itemid, row[0]);
		ZBX_STR2UCHAR(value_type, row[1]);

		switch (value_type)
		{
			case ITEM_VALUE_TYPE_FLOAT:
			case ITEM_VALUE_TYPE_UINT64:
				ZBX_STR2UCHAR(data_type, row[2]);
				formula = (1 == atoi(row[3]) ? row[4] : NULL);
				ZBX_STR2UCHAR(delta, row[5]);
				DBpatch_3030018_add_numeric_preproc_steps(&db_insert, itemid, data_type, formula,
						delta);
				break;
		}
	}

	DBfree_result(result);

	zbx_db_insert_autoincrement(&db_insert, "item_preprocid");
	ret = zbx_db_insert_execute(&db_insert);
	zbx_db_insert_clean(&db_insert);

	return ret;
}

static int	DBpatch_3030019(void)
{
	return DBdrop_field("items", "multiplier");
}

static int	DBpatch_3030020(void)
{
	return DBdrop_field("items", "data_type");
}

static int	DBpatch_3030021(void)
{
	return DBdrop_field("items", "delta");
}

static int	DBpatch_3030022(void)
{
	if (ZBX_DB_OK > DBexecute("update items set formula='' where flags<>1 or evaltype<>3"))
		return FAIL;

	return SUCCEED;
}

static int	DBpatch_3030023(void)
{
	if (ZBX_DB_OK > DBexecute("delete from profiles where idx like 'web.dashboard.widget.%%'"))
		return FAIL;

	return SUCCEED;
}

static int	DBpatch_3030024(void)
{
	const ZBX_FIELD	field = {"status", "0", NULL, NULL, 0, ZBX_TYPE_INT, ZBX_NOTNULL, 0};

	return DBadd_field("task", &field);
}

static int	DBpatch_3030025(void)
{
	const ZBX_FIELD	field = {"clock", "0", NULL, NULL, 0, ZBX_TYPE_INT, ZBX_NOTNULL, 0};

	return DBadd_field("task", &field);
}

static int	DBpatch_3030026(void)
{
	const ZBX_FIELD	field = {"ttl", "0", NULL, NULL, 0, ZBX_TYPE_INT, ZBX_NOTNULL, 0};

	return DBadd_field("task", &field);
}

static int	DBpatch_3030027(void)
{
	const ZBX_FIELD	field = {"proxy_hostid", NULL, NULL, NULL, 0, ZBX_TYPE_ID, 0, 0};

	return DBadd_field("task", &field);
}

static int	DBpatch_3030028(void)
{
	return DBcreate_index("task", "task_1", "status,proxy_hostid", 0);
}

static int	DBpatch_3030029(void)
{
	const ZBX_FIELD	field = {"proxy_hostid", NULL, "hosts", "hostid", 0, 0, 0, ZBX_FK_CASCADE_DELETE};

	return DBadd_foreign_key("task", 1, &field);
}

static int	DBpatch_3030030(void)
{
	const ZBX_TABLE table =
			{"task_remote_command", "taskid", 0,
				{
					{"taskid", NULL, NULL, NULL, 0, ZBX_TYPE_ID, ZBX_NOTNULL, 0},
					{"command_type", "0", NULL, NULL, 0, ZBX_TYPE_INT, ZBX_NOTNULL, 0},
					{"execute_on", "0", NULL, NULL, 0, ZBX_TYPE_INT, ZBX_NOTNULL, 0},
					{"port", "0", NULL, NULL, 0, ZBX_TYPE_INT, ZBX_NOTNULL, 0},
					{"authtype", "0", NULL, NULL, 0, ZBX_TYPE_INT, ZBX_NOTNULL, 0},
					{"username", "", NULL, NULL, 64, ZBX_TYPE_CHAR, ZBX_NOTNULL, 0},
					{"password", "", NULL, NULL, 64, ZBX_TYPE_CHAR, ZBX_NOTNULL, 0},
					{"publickey", "", NULL, NULL, 64, ZBX_TYPE_CHAR, ZBX_NOTNULL, 0},
					{"privatekey", "", NULL, NULL, 64, ZBX_TYPE_CHAR, ZBX_NOTNULL, 0},
					{"command", "", NULL, NULL, 0, ZBX_TYPE_SHORTTEXT, ZBX_NOTNULL, 0},
					{"alertid", NULL, NULL, NULL, 0, ZBX_TYPE_ID, 0, 0},
					{"parent_taskid", NULL, NULL, NULL, 0, ZBX_TYPE_ID, ZBX_NOTNULL, 0},
					{"hostid", NULL, NULL, NULL, 0, ZBX_TYPE_ID, ZBX_NOTNULL, 0},
					{0}
				},
				NULL
			};

	return DBcreate_table(&table);
}

static int	DBpatch_3030031(void)
{
	const ZBX_FIELD	field = {"taskid", NULL, "task", "taskid", 0, 0, 0, ZBX_FK_CASCADE_DELETE};

	return DBadd_foreign_key("task_remote_command", 1, &field);
}

static int	DBpatch_3030032(void)
{
	const ZBX_TABLE table =
			{"task_remote_command_result", "taskid", 0,
				{
					{"taskid", NULL, NULL, NULL, 0, ZBX_TYPE_ID, ZBX_NOTNULL, 0},
					{"status", "0", NULL, NULL, 0, ZBX_TYPE_INT, ZBX_NOTNULL, 0},
					{"parent_taskid", NULL, NULL, NULL, 0, ZBX_TYPE_ID, ZBX_NOTNULL, 0},
					{"info", "", NULL, NULL, 0, ZBX_TYPE_SHORTTEXT, ZBX_NOTNULL, 0},
					{0}
				},
				NULL
			};

	return DBcreate_table(&table);
}

static int	DBpatch_3030033(void)
{
	const ZBX_FIELD	field = {"taskid", NULL, "task", "taskid", 0, 0, 0, ZBX_FK_CASCADE_DELETE};

	return DBadd_foreign_key("task_remote_command_result", 1, &field);
}

static int	DBpatch_3030034(void)
{
	/* 1 - ZBX_TM_STATUS_NEW */
	if (ZBX_DB_OK > DBexecute("update task set status=1"))
		return FAIL;

	return SUCCEED;
}

static int	DBpatch_3030035(void)
{
	/* 2 - ZBX_SCRIPT_EXECUTE_ON_PROXY */
	const ZBX_FIELD field = {"execute_on", "2", NULL, NULL, 0, ZBX_TYPE_INT, ZBX_NOTNULL, 0};

	return DBset_default("scripts", &field);
}
#endif

DBPATCH_START(3030)

/* version, duplicates flag, mandatory flag */

DBPATCH_ADD(3030000, 0, 1)
DBPATCH_ADD(3030001, 0, 1)
DBPATCH_ADD(3030002, 0, 1)
DBPATCH_ADD(3030003, 0, 1)
DBPATCH_ADD(3030004, 0, 1)
DBPATCH_ADD(3030005, 0, 1)
DBPATCH_ADD(3030006, 0, 1)
DBPATCH_ADD(3030007, 0, 1)
DBPATCH_ADD(3030008, 0, 1)
DBPATCH_ADD(3030009, 0, 1)
DBPATCH_ADD(3030010, 0, 1)
DBPATCH_ADD(3030011, 0, 1)
DBPATCH_ADD(3030012, 0, 1)
DBPATCH_ADD(3030013, 0, 1)
DBPATCH_ADD(3030014, 0, 1)
DBPATCH_ADD(3030015, 0, 1)
DBPATCH_ADD(3030016, 0, 1)
DBPATCH_ADD(3030017, 0, 1)
DBPATCH_ADD(3030018, 0, 1)
DBPATCH_ADD(3030019, 0, 1)
DBPATCH_ADD(3030020, 0, 1)
DBPATCH_ADD(3030021, 0, 1)
DBPATCH_ADD(3030022, 0, 1)
DBPATCH_ADD(3030023, 0, 0)
DBPATCH_ADD(3030024, 0, 1)
DBPATCH_ADD(3030025, 0, 1)
DBPATCH_ADD(3030026, 0, 1)
DBPATCH_ADD(3030027, 0, 1)
DBPATCH_ADD(3030028, 0, 1)
DBPATCH_ADD(3030029, 0, 1)
DBPATCH_ADD(3030030, 0, 1)
DBPATCH_ADD(3030031, 0, 1)
DBPATCH_ADD(3030032, 0, 1)
DBPATCH_ADD(3030033, 0, 1)
DBPATCH_ADD(3030034, 0, 1)
DBPATCH_ADD(3030035, 0, 0)

DBPATCH_END()
