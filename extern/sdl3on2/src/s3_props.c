#include "s3_internal.h"

typedef enum S3_PropertyType {
    S3_PROPERTY_NUMBER,
    S3_PROPERTY_STRING,
    S3_PROPERTY_POINTER,
    S3_PROPERTY_BOOLEAN,
} S3_PropertyType;

typedef struct S3_Property {
    char* name;
    S3_PropertyType type;
    Sint64 number;
    char* string;
    void* pointer;
    bool boolean;
} S3_Property;

typedef struct S3_PropertyGroup {
    S3_PropertiesID id;
    S3_Property* properties;
    int count;
    int capacity;
} S3_PropertyGroup;

static S3_PropertyGroup* s3_groups;
static int s3_group_count;
static int s3_group_capacity;
static S3_PropertiesID s3_next_props_id = 1;

// SDL3 hands back the same id for repeated queries on one object.
typedef struct S3_ObjectProperties {
    const void* object;
    S3_PropertiesID props;
} S3_ObjectProperties;

#define S3_OBJECT_PROPERTIES_MAX 64

static S3_ObjectProperties s3_object_properties[S3_OBJECT_PROPERTIES_MAX];
static int s3_object_properties_count;

static S3_PropertyGroup* S3_FindGroup(S3_PropertiesID props)
{
    int index;

    for (index = 0; index < s3_group_count; index++) {
        if (s3_groups[index].id == props) {
            return &s3_groups[index];
        }
    }

    return NULL;
}

static S3_Property* S3_FindProperty(S3_PropertyGroup* group, const char* name)
{
    int index;

    for (index = 0; index < group->count; index++) {
        if (SDL_strcmp(group->properties[index].name, name) == 0) {
            return &group->properties[index];
        }
    }

    return NULL;
}

static S3_Property* S3_LookupProperty(S3_PropertiesID props, const char* name)
{
    S3_PropertyGroup* group = S3_FindGroup(props);

    if (group == NULL || name == NULL) {
        return NULL;
    }

    return S3_FindProperty(group, name);
}

static S3_Property* S3_AcquireProperty(S3_PropertiesID props, const char* name)
{
    S3_PropertyGroup* group;
    S3_Property* property;

    group = S3_FindGroup(props);
    if (group == NULL || name == NULL) {
        SDL_SetError("Invalid property group");
        return NULL;
    }

    property = S3_FindProperty(group, name);
    if (property != NULL) {
        SDL_free(property->string);
        property->string = NULL;
        return property;
    }

    if (group->count == group->capacity) {
        const int wanted = group->capacity != 0 ? group->capacity * 2 : 8;
        S3_Property* grown = (S3_Property*)SDL_realloc(group->properties, sizeof(*grown) * (size_t)wanted);

        if (grown == NULL) {
            SDL_SetError("Out of memory");
            return NULL;
        }

        group->properties = grown;
        group->capacity = wanted;
    }

    property = &group->properties[group->count];
    SDL_memset(property, 0, sizeof(*property));

    property->name = SDL_strdup(name);
    if (property->name == NULL) {
        SDL_SetError("Out of memory");
        return NULL;
    }

    group->count++;

    return property;
}

S3_PropertiesID S3_CreateProperties(void)
{
    S3_PropertyGroup* group;

    if (s3_group_count == s3_group_capacity) {
        const int wanted = s3_group_capacity != 0 ? s3_group_capacity * 2 : 8;
        S3_PropertyGroup* grown = (S3_PropertyGroup*)SDL_realloc(s3_groups, sizeof(*grown) * (size_t)wanted);

        if (grown == NULL) {
            SDL_SetError("Out of memory");
            return 0;
        }

        s3_groups = grown;
        s3_group_capacity = wanted;
    }

    group = &s3_groups[s3_group_count++];
    SDL_memset(group, 0, sizeof(*group));
    group->id = s3_next_props_id++;

    return group->id;
}

void S3_DestroyProperties(S3_PropertiesID props)
{
    S3_PropertyGroup* group;
    int index;

    group = S3_FindGroup(props);
    if (group == NULL) {
        return;
    }

    for (index = 0; index < group->count; index++) {
        SDL_free(group->properties[index].name);
        SDL_free(group->properties[index].string);
    }

    SDL_free(group->properties);

    *group = s3_groups[--s3_group_count];
}

bool S3_SetNumberProperty(S3_PropertiesID props, const char* name, Sint64 value)
{
    S3_Property* property = S3_AcquireProperty(props, name);

    if (property == NULL) {
        return false;
    }

    property->type = S3_PROPERTY_NUMBER;
    property->number = value;

    return true;
}

Sint64 S3_GetNumberProperty(S3_PropertiesID props, const char* name, Sint64 default_value)
{
    S3_Property* property = S3_LookupProperty(props, name);

    if (property == NULL) {
        return default_value;
    }

    switch (property->type) {
    case S3_PROPERTY_NUMBER:
        return property->number;
    case S3_PROPERTY_BOOLEAN:
        return property->boolean ? 1 : 0;
    case S3_PROPERTY_STRING:
        return property->string != NULL ? SDL_strtoll(property->string, NULL, 0) : default_value;
    default:
        return default_value;
    }
}

bool S3_SetStringProperty(S3_PropertiesID props, const char* name, const char* value)
{
    S3_Property* property;
    char* copy = NULL;

    // The new value may point into the existing property string.
    if (value != NULL) {
        copy = SDL_strdup(value);
        if (copy == NULL) {
            SDL_SetError("Out of memory");
            return false;
        }
    }

    property = S3_AcquireProperty(props, name);
    if (property == NULL) {
        SDL_free(copy);
        return false;
    }

    property->type = S3_PROPERTY_STRING;
    property->string = copy;

    return true;
}

const char* S3_GetStringProperty(S3_PropertiesID props, const char* name, const char* default_value)
{
    S3_Property* property = S3_LookupProperty(props, name);

    if (property == NULL || property->type != S3_PROPERTY_STRING || property->string == NULL) {
        return default_value;
    }

    return property->string;
}

bool S3_SetPointerProperty(S3_PropertiesID props, const char* name, void* value)
{
    S3_Property* property = S3_AcquireProperty(props, name);

    if (property == NULL) {
        return false;
    }

    property->type = S3_PROPERTY_POINTER;
    property->pointer = value;

    return true;
}

void* S3_GetPointerProperty(S3_PropertiesID props, const char* name, void* default_value)
{
    S3_Property* property = S3_LookupProperty(props, name);

    if (property == NULL || property->type != S3_PROPERTY_POINTER) {
        return default_value;
    }

    return property->pointer;
}

bool S3_SetBooleanProperty(S3_PropertiesID props, const char* name, bool value)
{
    S3_Property* property = S3_AcquireProperty(props, name);

    if (property == NULL) {
        return false;
    }

    property->type = S3_PROPERTY_BOOLEAN;
    property->boolean = value;

    return true;
}

bool S3_GetBooleanProperty(S3_PropertiesID props, const char* name, bool default_value)
{
    S3_Property* property = S3_LookupProperty(props, name);

    if (property == NULL) {
        return default_value;
    }

    switch (property->type) {
    case S3_PROPERTY_BOOLEAN:
        return property->boolean;
    case S3_PROPERTY_NUMBER:
        return property->number != 0;
    case S3_PROPERTY_STRING:
        if (property->string == NULL) {
            return default_value;
        }
        return SDL_strcmp(property->string, "0") != 0 && SDL_strcasecmp(property->string, "false") != 0;
    default:
        return default_value;
    }
}

S3_PropertiesID S3_AcquireObjectProperties(const void* object, bool* created)
{
    S3_PropertiesID props;
    int index;

    *created = false;

    if (object == NULL) {
        return 0;
    }

    for (index = 0; index < s3_object_properties_count; index++) {
        if (s3_object_properties[index].object == object) {
            return s3_object_properties[index].props;
        }
    }

    if (s3_object_properties_count >= S3_OBJECT_PROPERTIES_MAX) {
        SDL_SetError("Too many objects with properties");
        return 0;
    }

    props = S3_CreateProperties();
    if (props == 0) {
        return 0;
    }

    *created = true;

    s3_object_properties[s3_object_properties_count].object = object;
    s3_object_properties[s3_object_properties_count].props = props;
    s3_object_properties_count++;

    return props;
}

void S3_ReleaseObjectProperties(const void* object)
{
    int index;

    for (index = 0; index < s3_object_properties_count; index++) {
        if (s3_object_properties[index].object == object) {
            S3_DestroyProperties(s3_object_properties[index].props);
            s3_object_properties[index] = s3_object_properties[--s3_object_properties_count];
            return;
        }
    }
}

S3_PropertiesID S3_GetRendererProperties(S3_Renderer* renderer)
{
    SDL_RendererInfo info;
    S3_PropertiesID props;
    bool created;

    props = S3_AcquireObjectProperties(renderer, &created);
    if (props == 0 || !created) {
        return props;
    }

    if (SDL_GetRendererInfo(renderer, &info) == 0) {
        S3_SetStringProperty(props, S3_PROP_RENDERER_NAME_STRING, info.name);
    }

    return props;
}

S3_PropertiesID S3_GetTextureProperties(S3_Texture* texture)
{
    S3_PropertiesID props;
    bool created;
    Uint32 format;
    int access;
    int w;
    int h;

    props = S3_AcquireObjectProperties(texture, &created);
    if (props == 0 || !created) {
        return props;
    }

    if (SDL_QueryTexture(texture, &format, &access, &w, &h) == 0) {
        S3_SetNumberProperty(props, S3_PROP_TEXTURE_FORMAT_NUMBER, (Sint64)format);
        S3_SetNumberProperty(props, "SDL.texture.access", (Sint64)access);
        S3_SetNumberProperty(props, "SDL.texture.width", (Sint64)w);
        S3_SetNumberProperty(props, "SDL.texture.height", (Sint64)h);
    }

    return props;
}

void S3_QuitProperties(void)
{
    while (s3_group_count > 0) {
        S3_DestroyProperties(s3_groups[0].id);
    }

    SDL_free(s3_groups);
    s3_groups = NULL;
    s3_group_capacity = 0;
    s3_object_properties_count = 0;
}
