
#include "xcom.h"
#include "util.h"

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <locale>
#include <cassert>

using namespace xcom;

static std::string escape(const std::string& str) {
    std::string ret;

    for (size_t i = 0; i < str.length(); ++i) {
        switch (str[i])
        {
        case '"':
            ret += "\\\"";
            break;
        case '\\':
            ret += "\\\\";
            break;
        case '\n':
            ret += "\\n";
            break;
        case '\r':
            ret += "\\r";
            break;
        case '\t':
            ret += "\\t";
            break;
        default:
            if (str[i] > 0 && str[i] < ' ') {
                std::string hex = 
                    util::to_hex(reinterpret_cast<const unsigned char *>(&str[i]), 1);
                ret += "\\u00";
                ret += hex;
            }
            else {
                ret += str[i];
            }
        }
    }

    return ret;
}

struct json_writer
{
    json_writer(const std::string& filename) :
        out(filename), indent_level(0), skip_indent(true), needs_comma(false)
    {
        out.setf(std::ofstream::boolalpha);
    }

    void indent()
    {
        if (needs_comma) {
            out << ", ";

        }
        if (!skip_indent) {
            out << std::endl;
            std::string ind(2 * indent_level, ' ');
            out << ind;
        }
    }

    void begin_object(bool omit_newline = false)
    {
        indent();
        out << "{ ";
        ++indent_level;
        needs_comma = false;
        skip_indent = omit_newline;
    }

    void end_object()
    {
        --indent_level;
        if (needs_comma) {
            out << " ";
        }
        needs_comma = false;
        indent();
        out << "}";
        needs_comma = true;
        skip_indent = false;
    }

    void begin_array(bool omit_newline = false)
    {
        indent();
        out << "[ ";
        ++indent_level;
        needs_comma = false;
        skip_indent = omit_newline;
    }

    void end_array()
    {
        --indent_level;
        if (needs_comma) {
            out << " ";
        }
        needs_comma = false;
        indent();
        out << "]";
        needs_comma = true;
        skip_indent = false;
    }

    void end_item(bool omit_newline)
    {
        if (!omit_newline) {
            skip_indent = false;
        }
        else {
            skip_indent = true;
        }

        needs_comma = true;
    }

    void write_key(const std::string &name)
    {
        indent();
        out << "\"" << name << "\": ";
        skip_indent = true;
        needs_comma = false;
    }

    void write_int(const std::string &name, int32_t val, bool omit_newline = false)
    {
        write_key(name);
        out << val;
        end_item(omit_newline);
    }

    void write_raw_int(int val, bool omit_newline = false)
    {
        indent();
        out << val;
        end_item(omit_newline);
    }

    void write_float(const std::string &name, float val, bool omit_newline = false)
    {
        write_key(name);
        out << (val + 0.0f);
        end_item(omit_newline);
    }

    void write_raw_float(float val, bool omit_newline = false)
    {
        indent();
        out << val;
        end_item(omit_newline);
    }

    void write_string(const std::string &name, const std::string &val, 
            bool omit_newline = false)
    {
        write_key(name);
        out << "\"" << escape(val) << "\"";
        end_item(omit_newline);
    }

    void write_unicode_string(const std::string& name, const xcom_string& str)
    {
        write_key(name);
        begin_object(true);
        write_string("str", str.str, true);
        write_bool("is_wide", str.is_wide, true);
        end_object();
    }

    void write_raw_unicode_string(const xcom_string& str)
    {
        begin_object(true);
        write_string("str", str.str, true);
        write_bool("is_wide", str.is_wide, true);
        end_object();
    }

    void write_raw_string(const std::string& val, bool omit_newline = false)
    {
        indent();
        out << "\"" << escape(val) << "\"";
        end_item(omit_newline);
    }

    void write_bool(const std::string &name, bool val, bool omit_newline = false)
    {
        write_key(name);
        out << val;
        end_item(omit_newline);
    }


private:
    std::ofstream out;
    size_t indent_level;
    bool skip_indent;
    bool needs_comma;
};



struct json_property_visitor : public property_visitor
{
    json_property_visitor(json_writer &writer, const actor_table &ga, 
        const actor_table &la) : 
            w(writer), global_actors(ga), local_actors(la) {}

    void write_common(property* prop, bool omit_newline = false)
    {
        w.write_string("name", prop->name, omit_newline);
        w.write_string("kind", prop->kind_string(), omit_newline);
    }

    virtual void visit(int_property *prop) override
    {
        w.begin_object(true);
        write_common(prop, true);
        w.write_int("value", prop->value, true);
        w.end_object();
    }

    virtual void visit(float_property *prop) override
    {
        w.begin_object(true);
        write_common(prop, true);
        w.write_float("value", prop->value, true);
        w.end_object();
    }

    virtual void visit(bool_property *prop) override
    {
        w.begin_object(true);
        write_common(prop, true);
        w.write_bool("value", prop->value, true);
        w.end_object();
    }

    virtual void visit(string_property *prop) override
    {
        w.begin_object(true);
        write_common(prop, true);
        w.write_unicode_string("value", prop->str);
        w.end_object();
    }

    virtual void visit(name_property *prop) override
    {
        w.begin_object(true);
        write_common(prop, true);
        w.write_string("string", prop->str, true);
        w.write_int("number", prop->number, true);
        w.end_object();
    }

    virtual void visit(object_property *prop) override
    {
        w.begin_object(true);
        write_common(prop, true);
        w.write_int("actor", prop->actor, true);
        w.end_object();
    }

    virtual void visit(enum_property *prop) override
    {
        w.begin_object();
        write_common(prop);
        w.write_string("type", prop->type);
        w.write_string("value", prop->value.name);
        w.write_int("number", prop->value.number);
        w.end_object();
    }

    virtual void visit(struct_property *prop) override
    {
        w.begin_object();
        write_common(prop);
        w.write_string("struct_name", prop->struct_name);

        if (prop->native_data_length > 0) {
            w.write_string("native_data", 
                util::to_hex(prop->native_data.get(), prop->native_data_length));
            w.write_key("properties");
            w.begin_array(true);
            w.end_array();
        }
        else {
            w.write_string("native_data", "");
            w.write_key("properties");
            w.begin_array();
            std::for_each(prop->properties.begin(), prop->properties.end(),
                [this](const property_ptr& v) {
                    json_property_visitor visitor(*this);
                    v->accept(&visitor);
                });
            w.end_array();
        }
        w.end_object();
    }

    virtual void visit(array_property *prop) override
    {
        w.begin_object();
        write_common(prop);
        w.write_int("data_length", prop->data_length);
        w.write_int("array_bound", prop->array_bound);
        std::string data_str = (prop->array_bound > 0) ? 
            util::to_hex(prop->data.get(), prop->data_length) : "";
        w.write_string("data", data_str);
        w.end_object();
    }

    virtual void visit(object_array_property *prop) override
    {
        w.begin_object();
        write_common(prop);
        w.write_key("actors");
        w.begin_array(true);
        for (unsigned int i = 0; i < prop->elements.size(); ++i) {
            w.write_raw_int(prop->elements[i], true);
        }
        w.end_array();
        w.end_object();
    }

    virtual void visit(number_array_property *prop) override
    {
        w.begin_object();
        write_common(prop);
        w.write_key("elements");
        w.begin_array(true);
        for (int32_t v : prop->elements) {
            w.write_raw_int(v, true);
        }
        w.end_array();
        w.end_object();
    }

    virtual void visit(string_array_property *prop) override
    {
        w.begin_object();
        write_common(prop);
        w.write_key("strings");
        w.begin_array();
        for (const xcom_string& s : prop->elements) {
            w.write_raw_unicode_string(s);
        }
        w.end_array();
        w.end_object();
    }

    virtual void visit(enum_array_property *prop) override
    {
        w.begin_object();
        write_common(prop);
        w.write_key("enum_values");
        w.begin_array();
        for (const enum_value& s : prop->elements) {
            w.begin_object(true);
            w.write_string("value", s.name, true);
            w.write_int("number", s.number, true);
            w.end_object();
        }
        w.end_array();
        w.end_object();
    }

    virtual void visit(struct_array_property *prop) override
    {
        w.begin_object();
        write_common(prop);
        w.write_key("structs");
        w.begin_array();
        std::for_each(prop->elements.begin(), prop->elements.end(), 
            [this](const property_list& proplist) {
                w.begin_array();
                std::for_each(proplist.begin(), proplist.end(), 
                    [this](const property_ptr& p) {
                        p->accept(this);
                    });
                w.end_array();
            });

        w.end_array();
        w.end_object();
    }

    // Return true if we can condense a static array of strings into just a series of string literals in an array instead of having
    // a big array of nested string properties. This is the case as long as none of the strings in question are wide.
    // TODO: Probably need to revisit this for non-INT games where it's possibly a lot more likely that all the strings will be UTF-16.
    bool can_condense_string_array(const static_array_property& static_array)
    {
        for (const property_ptr& prop : static_array.properties) {
            const string_property *string_prop = dynamic_cast<const string_property*>(prop.get());
            assert(string_prop != nullptr);
            if (string_prop->str.is_wide) {
                return false;
            }
        }
        return true;
    }

    virtual void visit(static_array_property *prop) override
    {
        w.begin_object();
        write_common(prop);

        // If this array holds well-known subtypes (int), write them right in this node instead of repeating it
        if (prop->properties.size() > 0 && prop->properties[0]->kind == property::kind_t::int_property) {
            w.write_key("int_values");
            w.begin_array(true);
            for (const property_ptr& v : prop->properties) {
                const int_property *int_prop = dynamic_cast<const int_property*>(v.get());
                w.write_raw_int(int_prop->value, true);
            }
            w.end_array();
        }
        else if (prop->properties.size() > 0 && prop->properties[0]->kind == property::kind_t::string_property && can_condense_string_array(*prop)) {
            w.write_key("string_values");
            w.begin_array(true);
            for (const property_ptr& v : prop->properties) {
                const string_property *string_prop = dynamic_cast<const string_property*>(v.get());
                w.write_raw_string(string_prop->str.str, true);
            }
            w.end_array();
        }
        else {
            w.write_key("properties");
            w.begin_array();
            for (const property_ptr& v : prop->properties) {
                v->accept(this);
            }
            w.end_array();
        }

        w.end_object();
    }

    json_writer& w;
    const actor_table &global_actors;
    const actor_table &local_actors;
};

static void checkpoint_to_json(const checkpoint & chk, json_writer& w, 
    const actor_table& global_actors, const actor_table& local_actors)
{
    w.begin_object();
    w.write_string("name", chk.name);
    w.write_string("instance_name", chk.instance_name);
    w.write_string("class_name", chk.class_name);
    w.write_key("vector");
    w.begin_array(true);
    for (const auto& i : chk.vector) {
        w.write_raw_float(i, true);
    }
    w.end_array();
    w.write_key("rotator");
    w.begin_array(true);
    for (const auto& i : chk.rotator) {
        w.write_raw_int(i, true);
    }
    w.end_array();

    w.write_key("properties");
    w.begin_array();
    std::for_each(chk.properties.begin(), chk.properties.end(),
        [&w, &global_actors, &local_actors](const property_ptr& v) {
        json_property_visitor visitor{ w, global_actors, local_actors };
        v->accept(&visitor);
    });
    w.end_array();

    w.write_int("template_index", chk.template_index);
    w.write_int("pad_size", chk.pad_size);
    w.end_object();
}

static void checkpoint_chunk_to_json(const checkpoint_chunk& chk, 
    json_writer &w, const saved_game& save)
{
    w.begin_object();
    w.write_int("unknown_int1", chk.unknown_int1);
    w.write_string("game_type", chk.game_type);
    w.write_key("checkpoint_table");
    w.begin_array();
    std::for_each(chk.checkpoints.begin(), chk.checkpoints.end(),
        [&w, &save, &chk](const checkpoint& v) { 
            checkpoint_to_json(v, w, save.actors, chk.actors); 
        });
    w.end_array();

    w.write_int("unknown_int2", chk.unknown_int2);
    w.write_string("class_name", chk.class_name);

    w.write_key("actor_table");
    w.begin_array();

    std::for_each(chk.actors.begin(), chk.actors.end(),
        [&w](const std::string& a) { w.write_raw_string(a); }
    );
    w.end_array();

    w.write_int("unknown_int3", chk.unknown_int3);
    w.write_string("display_name", chk.display_name);
    w.write_string("map_name", chk.map_name);
    w.write_int("unknown_int4", chk.unknown_int4);
    w.end_object();
}

void buildJson(const saved_game& save, json_writer& w)
{
    w.begin_object();

    // Write the header
    const header &hdr = save.hdr;

    w.write_key("header");
    w.begin_object();
    w.write_int("version", static_cast<uint32_t>(hdr.version));
    w.write_int("uncompressed_size", hdr.uncompressed_size);
    w.write_int("game_number", hdr.game_number);
    w.write_int("save_number", hdr.save_number);
    w.write_unicode_string("save_description", hdr.save_description);
    w.write_unicode_string("time", hdr.time);
    w.write_string("map_command", hdr.map_command);
    w.write_bool("tactical_save", hdr.tactical_save);
    w.write_bool("ironman", hdr.ironman);
    w.write_bool("autosave", hdr.autosave);
    w.write_string("dlc", hdr.dlc);
    w.write_string("language", hdr.language);

    if (hdr.version == xcom_version::enemy_within_android) {
        w.write_int("profile_number", hdr.profile_number);
        w.write_unicode_string("profile_date", hdr.profile_date);
    }
    w.end_object();

    w.write_key("actor_table");
    w.begin_array();
    std::for_each(save.actors.begin(), save.actors.end(),
        [&w](const std::string& a) { w.write_raw_string(a); }
    );
    w.end_array();

    w.write_key("checkpoints");
    w.begin_array();
    std::for_each(save.checkpoints.begin(), save.checkpoints.end(),
        [&w, &save](const checkpoint_chunk& v) { 
            checkpoint_chunk_to_json(v, w, save); w.end_item(false); 
        });
    w.end_array();
    w.end_object();
}

void usage(const char * name)
{
    printf("xcom2json, a part of xcomsave, a save game editor for xcom games\n");
    printf("Created by tracktwo, with help from:\n");
    printf("\t shaygover, who added install instructions\n");
    printf("\t skywalkerytx, who fixed date problems\n");
    printf("\t golinski, who made the Enemy Unknow version\n");
    printf("\t Anders1232, who merged the Enemy Unknow version\n");
    printf("Usage: %s [-o <out_file>] <in_file>\n", name);
    printf("-o -- Specify output file name, defaults to <in_file>.json\n");
}


int main(int argc, char *argv[])
{
    std::string infile;
    std::string outfile;
    std::string tmpfile;

    if (argc <= 1) {
        usage(argv[0]);
        return 1;
    }

    setlocale(LC_ALL, "en_US.utf8");

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-o") == 0) {
            if (argc <= (i+1)) {
                usage(argv[0]);
                return 1;
            }
            outfile = argv[++i];
        }
        else {
            if (!infile.empty()) {
                usage(argv[0]);
                return 1;
            }

            infile = argv[i];
        }
    }

    if (infile.empty()) {
        usage(argv[0]);
        return 1;
    }

    if (outfile.empty()) {
        outfile = infile + ".json";
    }

    try {
        saved_game save = read_xcom_save(infile);
        json_writer w{ outfile };
        buildJson(save, w);
        return 0;
    }
    catch (const error::xcom_exception& e) {
        fprintf(stderr, "%s", e.what().c_str());
        return 1;
    }

    fprintf(stderr, "Error: unknown error.\n");
    return 1;
}
