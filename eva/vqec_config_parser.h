/*
 * vqec_config_parser.h - Implements parsing of VQE-C system configuration 
 * files.
 *
 * Copyright (c) 2007-2009 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "queue_plus.h"
#include "vam_types.h"

/**
 * Enumeration of setting types.
 */
typedef enum vqec_config_setting_type_ {
    VQEC_CONFIG_SETTING_TYPE_INVALID = 0,
    VQEC_CONFIG_SETTING_TYPE_STRING,
    VQEC_CONFIG_SETTING_TYPE_BOOLEAN,
    VQEC_CONFIG_SETTING_TYPE_INT,
    VQEC_CONFIG_SETTING_TYPE_LIST,
    VQEC_CONFIG_SETTING_TYPE_GROUP,
} vqec_config_setting_type_t;

#define VQEC_CONFIG_MAX_NAME_LEN 100

/**
 * Declaration for structure to be used with list element.
 */
struct vqec_config_setting_;

/**
 * Structure which contains all of a particular setting's data.
 */
typedef struct vqec_config_setting_ {
    /**
     * The type of setting which this is.
     */
    vqec_config_setting_type_t type;
    /**
     * The name of this particular setting.
     */
    char *name;
    /**
     * The following value fields are stored as a union to conserve memory.
     */
    union {
        /**
         * The string value of this setting.
         */
        char *value_string;
        /**
         * The boolean value of this setting.
         */
        boolean value_boolean;
        /**
         * The integer (signed) value of this setting.
         */
        int value_int;
    };

    /**
     * Queue object for setting list.
     */
    VQE_TAILQ_ENTRY(vqec_config_setting_) list_qobj;

    /**
     * Head for setting sublist.
     */
    VQE_TAILQ_HEAD(setting_sublist_head, vqec_config_setting_) subsetting_head;
} vqec_config_setting_t;

#define VQEC_CONFIG_ERROR_STRLEN 80

/**
 * Structure which contains all data within a particular configuration.
 */
typedef struct vqec_config_ {
    /**
     * Head for setting list.
     */
    vqec_config_setting_t root;
    /**
     * Textual information which may help indicate a problem in parsing a
     * particular configuration file.
     */
    char error_text[VQEC_CONFIG_ERROR_STRLEN];
    /**
     * Line number at which a problem occurred while parsing a particular
     * configuration file.
     */
    int error_line;
} vqec_config_t;


/**
 * Initialize the configuration parser.
 *
 * @param[in]  cfg  Instance of configuration parser.
 * @return  Returns TRUE if the parser was initialized successfully; FALSE
 *          otherwise.
 */
boolean vqec_config_init(vqec_config_t *cfg);

/**
 * Read a configuration file and parse its parameters and values.
 *
 * @param[in]  cfg  Instance of configuration parser.
 * @param[in]  filepath  Path to the file to be read and parsed.
 * @return  Returns TRUE if the file was read and parsed successfully; FALSE
 *          otherwise.  If FALSE is returned, the "error_text" and
 *          "error_line" fields of cfg may contain information helpful in
 *          determining what the problem was.
 */
boolean vqec_config_read_file(vqec_config_t *cfg,
                              const char *filepath);

/**
 * Read a buffer containing configuration information and parse its parameters
 * and values.
 *
 * @param[in]  cfg  Instance of configuration parser.
 * @param[in]  buffer  Pointer to the buffer to be parsed.
 * @return  Returns TRUE if the buffer was read and parsed successfully; FALSE
 *          otherwise.  If FALSE is returned, the "error_text" and
 *          "error_line" fields of cfg may contain information helpful in
 *          determining what the problem was.
 */
boolean vqec_config_read_buffer(vqec_config_t *cfg,
                                const char *buffer);

/**
 * Look up a configuration setting by its parameter name.
 *
 * @param[in]  cfg  Instance of configuration parser.
 * @param[in]  name  Name of the parameter which should be looked up.
 * @return  If a setting is found that matches the given parameter name, then
 *          a pointer to that setting is returned.  Otherwise, NULL is
 *          returned.
 */
vqec_config_setting_t *vqec_config_lookup(vqec_config_t *cfg,
                                          char *name);

/**
 * Determine the type of a particular setting.
 *
 * @param[in]  setting  Setting to have its type determined.
 * @return  Returns the type of the setting.
 */
vqec_config_setting_type_t
vqec_config_setting_type(vqec_config_setting_t *setting);

/**
 * Determine the length (number of elements) of a particular group or list
 * format configuration setting.  If the given setting is not a group or list
 * type, then 0 will be returned.
 *
 * @param[in]  setting  Setting to have its length determined.
 * @return  Returns the number of elements in the group or list.
 */
int vqec_config_setting_length(vqec_config_setting_t *setting);

/**
 * Retrieve an element of a list by its index.  If the given setting is not a
 * list type, then NULL will be returned.
 *
 * @param[in]  setting  List from which the element shall be retrieved.
 * @param[in]  index  Index of the requested element within the list.
 * @return  Returns a pointer to the requested element if it exists.  If the
 *          requested element does not exist, NULL is returned.
 */
vqec_config_setting_t *
vqec_config_setting_get_elem(vqec_config_setting_t *setting,
                             int index);

/**
 * Retrieve a member of a group by its name.  If the given setting is not a
 * group type, then NULL will be returned.
 *
 * @param[in]  setting  Group from which the member shall be retrieved.
 * @param[in]  name  Name of the requested member within the group.
 * @return  Returns a pointer to the member with the given name if it exists.
 *          If no member with the given name exists, NULL is returned.
 */
vqec_config_setting_t *
vqec_config_setting_get_member(vqec_config_setting_t *setting,
                               char *name);

/**
 * Retrieve the value of a string setting.  If the given setting is not a
 * string type, then NULL will be returned.
 *
 * @param[in]  setting  Setting to have its value retrieved.
 * @return  Returns a pointer to the string value of the setting.
 */
char *vqec_config_setting_get_string(vqec_config_setting_t *setting);

/**
 * Retrieve the value of a boolean setting.  If the given setting is not a
 * boolean type, then FALSE will be returned.
 *
 * @param[in]  setting  Setting to have its value retrieved.
 * @return  Returns TRUE or FALSE in accordance with the value of the setting.
 */
boolean vqec_config_setting_get_bool(vqec_config_setting_t *setting);

/**
 * Retrieve the value of a signed integer setting.  If the given setting is not
 * an integer type, then 0 will be returned.
 *
 * @param[in]  setting  Setting to have its value retrieved.
 * @return  Returns the signed integer value of the setting.
 */
int vqec_config_setting_get_int(vqec_config_setting_t *setting);

/**
 * Destroy all information stored in a configuration parser instance.
 *
 * @param[in]  cfg  Instance of configuration parser.
 * @return  Returns TRUE if the parser was destroyed successfully; FALSE
 *          otherwise.
 */
boolean vqec_config_destroy(vqec_config_t *cfg);
