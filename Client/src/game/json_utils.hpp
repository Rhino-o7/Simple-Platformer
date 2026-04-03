#pragma once

#ifndef _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#endif
#ifndef _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
#define _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
#endif

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#include <string>

namespace game::json_utils {
    using JsonValue = rapidjson::Value;
    using JsonDocument = rapidjson::Document;

    inline bool parse_document(const std::string& json, JsonDocument& out, std::string& error) {
        std::string content = json;
        if (content.size() >= 3
            && (unsigned char)content[0] == 0xEF
            && (unsigned char)content[1] == 0xBB
            && (unsigned char)content[2] == 0xBF) {
            content.erase(0, 3);
        }

        out.Parse(content.c_str());
        if (out.HasParseError()) {
            error = "Invalid JSON at offset " + std::to_string(out.GetErrorOffset()) + ": "
                + rapidjson::GetParseError_En(out.GetParseError());
            return false;
        }

        return true;
    }

    inline const JsonValue* get_field(const JsonValue& object, const char* key) {
        if (!object.IsObject()) {
            return nullptr;
        }

        auto it = object.FindMember(key);
        if (it == object.MemberEnd()) {
            return nullptr;
        }

        return &it->value;
    }

    inline bool as_string(const JsonValue* value, std::string& out) {
        if (value == nullptr || !value->IsString()) {
            return false;
        }

        out = value->GetString();
        return true;
    }

    inline bool as_double(const JsonValue* value, double& out) {
        if (value == nullptr || !value->IsNumber()) {
            return false;
        }

        out = value->GetDouble();
        return true;
    }

    inline bool as_float(const JsonValue* value, float& out) {
        if (value == nullptr || !value->IsNumber()) {
            return false;
        }

        out = (float)value->GetDouble();
        return true;
    }

    inline bool as_vec3(const JsonValue* value, glm::vec3& out) {
        if (value == nullptr || !value->IsArray() || value->Size() != 3) {
            return false;
        }

        for (rapidjson::SizeType i = 0; i < 3; ++i) {
            if (!(*value)[i].IsNumber()) {
                return false;
            }
        }

        out = {
            (float)(*value)[0].GetDouble(),
            (float)(*value)[1].GetDouble(),
            (float)(*value)[2].GetDouble(),
        };
        return true;
    }

    inline bool as_quat_xyzw(const JsonValue* value, glm::quat& out) {
        if (value == nullptr || !value->IsArray() || value->Size() != 4) {
            return false;
        }

        for (rapidjson::SizeType i = 0; i < 4; ++i) {
            if (!(*value)[i].IsNumber()) {
                return false;
            }
        }

        const float x = (float)(*value)[0].GetDouble();
        const float y = (float)(*value)[1].GetDouble();
        const float z = (float)(*value)[2].GetDouble();
        const float w = (float)(*value)[3].GetDouble();
        out = glm::quat(w, x, y, z);
        return true;
    }

    inline bool as_quat_wxyz(const JsonValue* value, glm::quat& out) {
        if (value == nullptr || !value->IsArray() || value->Size() != 4) {
            return false;
        }

        for (rapidjson::SizeType i = 0; i < 4; ++i) {
            if (!(*value)[i].IsNumber()) {
                return false;
            }
        }

        const float w = (float)(*value)[0].GetDouble();
        const float x = (float)(*value)[1].GetDouble();
        const float y = (float)(*value)[2].GetDouble();
        const float z = (float)(*value)[3].GetDouble();
        out = glm::quat(w, x, y, z);
        return true;
    }
}
