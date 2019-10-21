#include <json-parser/json.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <string.h>

#include "queue_t.h"

// BFS
json_value *find_value(json_value *u, const char *name) {
    assert(u != NULL);
    assert(name != NULL);

    json_value *r = NULL;
    queue_t *queue = queue_constructor();
    queue_push(queue, u);

    while (!queue_is_empty(queue) && r == NULL) {
        u = queue_front(queue);
        queue_pop(queue);

        switch (u->type) {
            case json_object:
                for (int i = 0; i < u->u.object.length && r == NULL; i++) {
                    json_object_entry *entry = &(u->u.object.values[i]);
                    queue_push(queue, entry->value);

                    if (strcmp(entry->name, name) == 0) {
                        r = entry->value;
                    }
                }
                break;
            case json_array:
                for (int i = 0; i < u->u.array.length; i++) {
                    queue_push(queue, u->u.array.values[i]);
                }
                break;
            case json_integer:
            case json_double:
            case json_string:
            case json_boolean:
            case json_null:
            default:
                break;
        }
    }

    queue_destructor(queue, 0);

    return r;
}

#ifdef __cplusplus
}
#endif