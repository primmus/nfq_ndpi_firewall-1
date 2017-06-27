#include <pcre.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "rule_helper.h"

const char *ip_pattern = "(\\d+)\\.(\\d+)\\.(\\d+)\\.(\\d+)";

int validate_ip_string(char *string)
{
    if (strcmp(string, "any") == 0) {
	return 1;
    }

    pcre *re_compiled;
    pcre_extra *pcre_ex;
    const char pcre_error_str;
    int pcre_error_offset;
    int pcre_exec_ret;
    int sub_str_vec[30];

    int ret = 0;

    re_compiled = pcre_compile(ip_pattern, 0, &pcre_error_str, &pcre_error_offset, NULL);

    if (re_compiled == NULL) {
	printf("ERROR: could not compile '%s': %s\n", ip_pattern, pcre_error_str);
	exit(1);
    }

    pcre_ex = pcre_study(re_compiled, 0, &pcre_error_str);
    if (pcre_error_str != NULL) {
	printf("ERROR: Could not study '%s': %s\n", ip_pattern, pcre_error_str);
    }

    pcre_exec_ret = pcre_exec(re_compiled, pcre_ex, string, strlen(string), 0, 0, sub_str_vec, 30);

    if (pcre_exec_ret > 0) {
	ret = 1;
    }

    return ret;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
	printf("USAGE: RuleManager <rulesfile> <action> [action params]\n");
	exit(1);
    }
    
    char *filename = argv[1];
    char action = argv[2][0];
    struct Connection *conn = rules_open(filename, action);
    
    unsigned short dport;
    int policy;
    char *src, *dst, *app, *policy_str;

    int id = 0;
    if (argc > 3) {
	id = atoi(argv[3]);
    }
    
    if (id >= MAX_RULES) {
	die("There's not so many records.", conn);
    }
    
    switch(action) {
	case 'A':
	if (argc != 8) {
	    die("Need src, dst, dport, app and policy to append.", conn);
	}

	id = get_rules_num(conn);

	if (id >= MAX_RULES) {
	    die("Maximum number of rules has been reached, exiting.", conn);
	}
		
	src = argv[3];
	dst = argv[4];
	dport = (unsigned short)atoi(argv[5]);
	app = argv[6];
	policy_str = argv[7];

	if (validate_ip_string(src) == 0) {
	    printf("ERROR: '%s' ip is invalid.\n", src);
	    die("Enter a valid IP.", conn);
	}

	if (validate_ip_string(dst) == 0) {
	    printf("ERROR: '%s' IP is invalid.\n", dst);
	    die("Enter a valid IP.", conn);
	}

	policy = set_policy(policy_str);
	if (policy == 0) {
	    printf ("%s is an invalid policy.\n", policy_str);
	    die("Valid options are: 'ALLOW', 'DENY', 'REJECT', 'ALLOW with IPS'.", conn);
	}

	rule_set(conn, id, src, dst, dport, app, policy);
	rules_write(conn);
	break;
    case 'c':
	    rules_create(conn);
	    rules_write(conn);
	    break;
    case 'd':
	if (argc != 4) {
	    die("Need id to delete.", conn);
	}
    
	rule_delete(conn, id);
	rules_write(conn);
	break;
    case 'g':
	if (argc != 4) {
	    die("Need an id to get.", conn);
	}
    
	rule_get(conn, id);
	break;
    case 'I':
	if (argc == 8) {
	    id = 0;
	    src = argv[3];
	    dst = argv[4];
	    dport = (unsigned short)atoi(argv[5]);
	    app = argv[6];
	    policy_str = argv[7];
	} else if (argc == 9) {
	    src = argv[4];
	    dst = argv[5];
	    dport = (unsigned short)atoi(argv[6]);
	    app = argv[7];
	    policy_str = argv[8];
	} else {
	    die("Need [id], src, dst, dport, app, policy to insert.", conn);
	}

	if (validate_ip_string(src) == 0) {
	    printf("ERROR: '%s' ip is invalid.\n", src);
	    die("Enter a valid IP.", conn);
	}

	if (validate_ip_string(dst) == 0) {
	    printf("ERROR: '%s' IP is invalid.\n", dst);
	    die("Enter a valid IP.", conn);
	}

	policy = set_policy(policy_str);
	if (policy == 0) {
	    printf ("%s is an invalid policy.\n", policy_str);
	    die("Valid options are: 'ALLOW', 'DENY', 'REJECT', 'ALLOW with IPS'.", conn);
	}

	int num_of_rules = get_rules_num(conn);
	if (num_of_rules >= MAX_RULES) {
	    die("Maximum number of rules reached.", conn);
	}

	// create temporary storage for rules
	struct Rules *rules = malloc(sizeof(*rules));
	int q = 0;
	for (q = 0; q < MAX_RULES; q++) {
	    struct Rule rule = { .set = 0 };
	    rules->rules[q] = rule;
	}

	// copy rules before id as is
	for (q = 0; q < id; q++) {
	    rules->rules[q] = conn->rules->rules[q];
	}
	
	// copy rules after id with updated ids
	for (q = num_of_rules; q > id; q--) {
	    rules->rules[q] = conn->rules->rules[q - 1];
	}
	
	// copy new rule
	rules->rules[id].set = 1;
	
	// set src
	char *res = strncpy(rules->rules[id].src, src, strlen(src)); // 16 is the IP address + \0
	rules->rules[id].src[sizeof(rules->rules[id].src) - 1] = '\0';
        
	if (!res) {
	    die("Source copy failed.", conn);
        }
        
	// set dst
        res = strncpy(rules->rules[id].dst, dst, strlen(dst));
        rules->rules[id].dst[sizeof(rules->rules[id].dst) - 1] = '\0';
        
	if (!res) {
	    die("Destination copy failed.", conn);
        }
        
	// set dport
        rules->rules[id].dport = dport;
        
	// set app
        res = strncpy(rules->rules[id].app, app, MAX_DATA);
        rules->rules[id].app[sizeof(rules->rules[id].app) - 1] = '\0';
        
	if (!res) {
	    die("Application copy failed.", conn);
        }
        
	//set policy
        rules->rules[id].policy = policy;

	conn->rules = rules;
	
	rules_write(conn);
	break;
    case 'l':
	rules_list(conn);
	break;
    case 's':
	if (argc != 9) {
	    die("Need id, src, dst, dport, app and policy to set.", conn);
	}
	
	src = argv[4];
	dst = argv[5];
	dport = (unsigned short)atoi(argv[6]);
	app = argv[7];
	policy_str = argv[8];

	if (validate_ip_string(src) == 0) {
	    printf("ERROR: '%s' ip is invalid.\n", src);
	    die("Enter a valid IP.", conn);
	}

	if (validate_ip_string(dst) == 0) {
	    printf("ERROR: '%s' ip is invalid.\n", dst);
	    die("Enter a valid IP.", conn);
	}

	policy = set_policy(policy_str);
	if (policy == 0) {
	    printf ("%s is an invalid policy.\n", policy_str);
	    die("Valid options are: 'ALLOW', 'DENY', 'REJECT', 'ALLOW with IPS'.", conn);
	}

	rule_set(conn, id, src, dst, dport, app, policy);
	rules_write(conn);
	break;
    default:
	die("Invalid action: A=append, c=create, d = delete, g=get, I=insert, l=list, s=set", conn);
    }
    
    rules_close(conn);
    return 0;
}
