#define SET_BOOL(domain_str, settings_obj, struct_field) \
    if (streq(domain, domain_str) && streq(key, #struct_field) && val) { \
        settings_obj.struct_field = (streq(val, "true") || streq(val, "1")); \
        matched = 1; \
    }

#define SET_STRING(domain_str, settings_obj, struct_field) \
    if (streq(domain, domain_str) && streq(key, #struct_field) && val) { \
        settings_obj.struct_field = val; \
        matched = 1; \
    }
