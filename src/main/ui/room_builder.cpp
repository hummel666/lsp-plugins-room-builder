/*
 * Copyright (C) 2021 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2021 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugins-room-builder
 * Created on: 3 авг. 2021 г.
 *
 * lsp-plugins-room-builder is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * lsp-plugins-room-builder is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with lsp-plugins-room-builder. If not, see <https://www.gnu.org/licenses/>.
 */

#include <private/meta/room_builder.h>
#include <private/ui/room_builder.h>
#include <lsp-plug.in/plug-fw/ui.h>
#include <lsp-plug.in/plug-fw/meta/func.h>
#include <lsp-plug.in/stdlib/stdio.h>

namespace lsp
{
    namespace plugui
    {
        //---------------------------------------------------------------------
        // Plugin UI factory
        static const meta::plugin_t *plugin_uis[] =
        {
            &meta::room_builder_mono,
            &meta::room_builder_stereo
        };

        static ui::Module *ui_factory(const meta::plugin_t *meta)
        {
            return new room_builder_ui(meta);
        }

        static ui::Factory factory(ui_factory, plugin_uis, 2);

        //---------------------------------------------------------------------
        room_builder_ui::CtlFloatPort::CtlFloatPort(room_builder_ui *ui, const char *pattern, const meta::port_t *meta):
            ui::IPort(meta)
        {
            pUI         = ui;
            sPattern    = pattern;

            char name[0x100];
            ::sprintf(name, "/scene/object/*/%s", sPattern);
            osc::pattern_create(&sOscPattern, name);
            fValue      = default_value();
        }

        room_builder_ui::CtlFloatPort::~CtlFloatPort()
        {
            pUI         = NULL;
            sPattern    = NULL;
            osc::pattern_destroy(&sOscPattern);
        }

        const char *room_builder_ui::CtlFloatPort::name()
        {
            const char *format = NULL;
            return (osc::pattern_get_format(&sOscPattern, &format) == STATUS_OK) ? format : NULL;
        }

        float room_builder_ui::CtlFloatPort::value()
        {
            // Prepare the value
            char name[0x100];
            float value = 0.0f;
            ::sprintf(name, "/scene/object/%d/%s", int(pUI->nSelected), sPattern);

            // Fetch value
            core::KVTStorage *kvt = pUI->wrapper()->kvt_lock();
            status_t res = STATUS_NOT_FOUND;
            if (kvt != NULL)
            {
                res = kvt->get(name, &value);
                pUI->wrapper()->kvt_release();
            }

            // Return the limited value
            return fValue = (res == STATUS_OK) ?
                    meta::limit_value(pMetadata, value) :
                    default_value();
        }

        void room_builder_ui::CtlFloatPort::set_value(float value)
        {
            // Do not update value
            if (fValue == value)
                return;

            // Prepare the value
            char name[0x100];
            sprintf(name, "/scene/object/%d/%s", int(pUI->nSelected), sPattern);
            value       = limit_value(pMetadata, value);

            // Obtain KVT storage
            core::KVTStorage *kvt = pUI->wrapper()->kvt_lock();
            if (kvt != NULL)
            {
                core::kvt_param_t param;
                param.type  = core::KVT_FLOAT32;
                param.f32   = value;

                // Write in silent mode
                if (kvt->put(name, &param, core::KVT_TO_DSP) == STATUS_OK)
                {
                    fValue = value;
                    pUI->wrapper()->kvt_notify_write(kvt, name, &param);
                }
                pUI->wrapper()->kvt_release();
            }
        }

        bool room_builder_ui::CtlFloatPort::match(const char *id)
        {
            return (osc::pattern_match(&sOscPattern, id) == STATUS_OK);
        }

        bool room_builder_ui::CtlFloatPort::changed(core::KVTStorage *storage, const char *id, const core::kvt_param_t *value)
        {
            char name[0x100];
            ::sprintf(name, "/scene/object/%d/%s", int(pUI->nSelected), sPattern);
            if (::strcmp(name, id) != 0)
                return false;

            notify_all();
            return true;
        }

        //---------------------------------------------------------------------
        static const char *UNNAMED_STR = "<unnamed>";

        room_builder_ui::CtlListPort::CtlListPort(room_builder_ui *ui, const meta::port_t *meta):
            ui::IPort(&sMetadata)
        {
            pUI         = ui;
            sMetadata   = *meta;
            nItems      = 0;
            nCapacity   = 0;
            pItems      = NULL;
            nSelectedReq= -1;

            osc::pattern_create(&sOscPattern, "/scene/object/*/name");
        }

        room_builder_ui::CtlListPort::~CtlListPort()
        {
            vKvtPorts.flush();

            if (pItems != NULL)
            {
                for (size_t i=0; i<nCapacity; ++i)
                {
                    char *s = const_cast<char *>(pItems[i].text);
                    if ((s != NULL) && (s != UNNAMED_STR))
                        ::free(s);
                    pItems[i].text = NULL;
                }

                ::free(pItems);
                pItems      = NULL;
            }

            osc::pattern_destroy(&sOscPattern);
        }

        const char *room_builder_ui::CtlListPort::name()
        {
            return "/scene/objects";
        }

        float room_builder_ui::CtlListPort::value()
        {
            ssize_t index = pUI->nSelected;
            if (nItems > 0)
            {
                if (index >= ssize_t(nItems))
                    index   = nItems-1;
                else if (index < 0)
                    index   = 0;
            }
            else
                index = -1;

            return index;
        }

        void room_builder_ui::CtlListPort::set_value(float value)
        {
            ssize_t index   = value;
            if (index == pUI->nSelected)
                return;

            pUI->nSelected  = index;

            // Deploy new value to KVT
            core::KVTStorage *kvt = pUI->wrapper()->kvt_lock();
            if (kvt != NULL)
            {
                core::kvt_param_t p;
                p.type      = core::KVT_FLOAT32;
                p.f32       = index;
                kvt->put("/scene/selected", &p, core::KVT_RX);
                pUI->wrapper()->kvt_notify_write(kvt, "/scene/selected", &p);
                pUI->wrapper()->kvt_release();
            }

            // Notify all KVT ports
            for (size_t i=0, n=vKvtPorts.size(); i<n; ++i)
            {
                ui::IPort *p = vKvtPorts.get(i);
                if (p != NULL)
                    p->notify_all();
            }
        }

        void room_builder_ui::CtlListPort::add_port(ui::IPort *port)
        {
            vKvtPorts.add(port);
        }

        void room_builder_ui::CtlListPort::set_list_item(size_t id, const char *value)
        {
            if (pItems == NULL)
                return;
            char **v = const_cast<char **>(&pItems[id].text);

            // Free previous value holder
            if ((*v != NULL) && (*v != UNNAMED_STR))
                ::free(*v);

            // Try to copy name of parameter
            if (value != NULL)
                *v = ::strdup(value);
            else if (::asprintf(v, "<unnamed #%d>", int(id)) < 0)
                *v  = NULL;

            // If all is bad, do this
            if (*v == NULL)
                *v  = const_cast<char *>(UNNAMED_STR);
        }

        bool room_builder_ui::CtlListPort::match(const char *id)
        {
            if (!strcmp(id, "/scene/objects"))
                return true;
            if (!strcmp(id, "/scene/selected"))
                return true;
            return osc::pattern_match(&sOscPattern, id);
        }

        bool room_builder_ui::CtlListPort::changed(core::KVTStorage *storage, const char *id, const core::kvt_param_t *value)
        {
            if ((value->type == core::KVT_INT32) && (!strcmp(id, "/scene/objects")))
            {
                // Ensure that we have enough place to store object names
                size_t size     = (value->i32 < 0) ? 0 : value->i32;
                if (nItems == size)
                    return false;

                // Compute the capacity and adjust array size
                size_t capacity = ((size + 0x10) / 0x10) * 0x10;
                if (capacity > nCapacity)
                {
                    meta::port_item_t *list = reinterpret_cast<meta::port_item_t *>(::realloc(pItems, capacity * sizeof(meta::port_item_t)));
                    if (list == NULL)
                        return false;
                    for (size_t i=nCapacity; i<capacity; ++i)
                    {
                        list[i].text    = NULL;
                        list[i].lc_key  = NULL;
                    }

                    pItems          = list;
                    nCapacity       = capacity;
                    sMetadata.items = list;
                }

                // Allocate non-allocated strings
                char pname[0x100]; // Should be enough
                for (size_t i=nItems; i < size; ++i)
                {
                    ::snprintf(pname, sizeof(pname), "/scene/object/%d/name", int(i));
                    const char *pval = NULL;
                    status_t res = storage->get(pname, &pval);
                    set_list_item(i, (res == STATUS_OK) ? pval : NULL);
                }
                nItems  = size; // Update size

                // Set the end of string list
                char **s = const_cast<char **>(&pItems[nItems].text);
                if ((*s != NULL) && (*s != UNNAMED_STR))
                    ::free(*s);
                *s = NULL;

                // Cleanup storage
                kvt_cleanup_objects(storage, nItems);

                // Change selected value
                ssize_t index = pUI->nSelected;
                if (storage->get(id, &value) == STATUS_OK)
                {
                    if (value->type == core::KVT_FLOAT32)
                        index   = value->f32;
                }

                if (index < 0)
                    index = 0;
                else if (index >= ssize_t(nItems))
                    index = nItems-1;
                set_value(index);           // Update the current selected value

                sync_metadata();            // Call for metadata update
                notify_all();               // Notify all bound listeners
                return true;
            }
            else if ((value->type == core::KVT_FLOAT32) && (!strcmp(id, "/scene/selected")))
            {
                set_value(value->f32);
            }
            else if ((value->type == core::KVT_STRING) && (::strstr(id, "/scene/object/") == id))
            {
                id += ::strlen("/scene/object/");

                char *endptr = NULL;
                errno = 0;
                long index = ::strtol(id, &endptr, 10);

                // Valid object number?
                if ((errno == 0) && (!::strcmp(endptr, "/name")) &&
                    (index >= 0) && (index < ssize_t(nItems)))
                {
                    set_list_item(index, value->str);   // Update list element
                    sync_metadata();                    // Synchronize metadata
                    return true;
                }
            }

            return false;
        }

        room_builder_ui::CtlMaterialPreset::CtlMaterialPreset(room_builder_ui *ui)
        {
            pUI         = ui;
            pCBox       = NULL;
            hHandler    = -1;
            pSpeed      = NULL;
            pAbsorption = NULL;
            pSelected   = NULL;
        }

        room_builder_ui::CtlMaterialPreset::~CtlMaterialPreset()
        {
            pSpeed      = NULL;
            pAbsorption = NULL;
            pSelected   = NULL;
        }

        void room_builder_ui::CtlMaterialPreset::init(const char *preset, const char *selected, const char *speed, const char *absorption)
        {
            // Just bind ports
            pSpeed      = pUI->wrapper()->port(speed);
            pAbsorption = pUI->wrapper()->port(absorption);
            pSelected   = pUI->wrapper()->port(selected);

            // Fetch widget
            pCBox       = tk::widget_cast<tk::ComboBox>(pUI->wrapper()->controller()->widgets()->find("mpreset"));

            // Initialize list of presets
            tk::ListBoxItem *li;
            LSPString lc;
            if (pCBox != NULL)
            {
                // Initialize box
                li = new tk::ListBoxItem(pCBox->display());
                if (li == NULL)
                    return;

                li->init();
                li->text()->set("lists.room_bld.select_mat");
                li->tag()->set(-1);
                pCBox->items()->madd(li);
                pCBox->selected()->set(li);

                size_t i=0;
                for (const meta::room_material_t *m = meta::room_builder_metadata::materials; m->name != NULL; ++m)
                {
                    li = new tk::ListBoxItem(pCBox->display());
                    if (li == NULL)
                        return;

                    li->init();
                    if (m->lc_key != NULL)
                    {
                        lc.set_ascii("lists.");
                        lc.append_ascii(m->lc_key);
                        li->text()->set(&lc);
                    }
                    else
                        li->text()->set_raw(m->name);
                    li->tag()->set(i++);
                    pCBox->items()->madd(li);
                }

                // Bind listener
                hHandler    = pCBox->slots()->bind(tk::SLOT_SUBMIT, slot_change, this);
            }

            // Bind handlers and notify changes
            if (pSpeed != NULL)
            {
                pSpeed->bind(this);
                pSpeed->notify_all();
            }
            if (pAbsorption != NULL)
            {
                pAbsorption->bind(this);
                pAbsorption->notify_all();
            }
            if (pSelected != NULL)
            {
                pSelected->bind(this);
                pSelected->notify_all();
            }
        }

        status_t room_builder_ui::CtlMaterialPreset::slot_change(tk::Widget *sender, void *ptr, void *data)
        {
            CtlMaterialPreset *_this = static_cast<CtlMaterialPreset *>(ptr);
            if (ptr == NULL)
                return STATUS_BAD_STATE;

            ssize_t sel = _this->pSelected->value();
            if (sel < 0)
                return STATUS_OK;

            tk::ListBoxItem *li = _this->pCBox->selected()->get();
            ssize_t idx = (li != NULL) ? li->tag()->get() : -1;
            if (idx < 0)
                return STATUS_OK;

            const meta::room_material_t *m = &meta::room_builder_metadata::materials[idx];

            if (_this->pAbsorption->value() != m->absorption)
            {
                _this->pAbsorption->set_value(m->absorption);
                _this->pAbsorption->notify_all();
            }

            if (_this->pSpeed->value() != m->speed)
            {
                _this->pSpeed->set_value(m->speed);
                _this->pSpeed->notify_all();
            }

            return STATUS_OK;
        }

        void room_builder_ui::CtlMaterialPreset::notify(ui::IPort *port)
        {
            if (pCBox == NULL)
                return;

            float fAbsorption   = pAbsorption->value();
            float fSpeed        = pSpeed->value();

            // Find best match
            ssize_t idx = -1, i = 0;
            for (const meta::room_material_t *m = meta::room_builder_metadata::materials; m->name != NULL; ++m, ++i)
            {
                if ((m->speed == fSpeed) && (m->absorption == fAbsorption))
                {
                    idx = i;
                    break;
                }
            }

            // Set-up selected index in non-notify mode
            tk::ListBoxItem *li = pCBox->selected()->get();

            if ((li == NULL) || (li->tag()->get() != idx))
            {
                li = pCBox->items()->get(idx);

                pCBox->slots()->disable(tk::SLOT_SUBMIT, hHandler);
                pCBox->selected()->set(li);
                pCBox->slots()->enable(tk::SLOT_SUBMIT, hHandler);
            }
        }

        room_builder_ui::CtlKnobBinding::CtlKnobBinding(room_builder_ui *ui, bool reverse)
        {
            pUI         = ui;
            pOuter      = NULL;
            pInner      = NULL;
            pLink       = NULL;
            bReverse    = reverse;
        }

        room_builder_ui::CtlKnobBinding::~CtlKnobBinding()
        {
            pUI         = NULL;
            pOuter      = NULL;
            pInner      = NULL;
            pLink       = NULL;
            bReverse    = false;
        }

        void room_builder_ui::CtlKnobBinding::init(const char *outer, const char *inner, const char *link)
        {
            // Just bind ports
            pOuter      = pUI->wrapper()->port(outer);
            pInner      = pUI->wrapper()->port(inner);
            pLink       = pUI->wrapper()->port(link);

            // Bind handlers and notify changes
            if (pLink != NULL)
            {
                pLink->bind(this);
                pLink->notify_all();
            }
            if (pInner != NULL)
            {
                pInner->bind(this);
                pInner->notify_all();
            }
            if (pOuter != NULL)
            {
                pOuter->bind(this);
                pOuter->notify_all();
            }
        }

        void room_builder_ui::CtlKnobBinding::notify(ui::IPort *port)
        {
            if (port == NULL)
                return;

            bool link = (pLink != NULL) ? pLink->value() >= 0.5f : false;
            if (!link)
                return;

            if (port == pLink)
                port = pOuter;

            if ((port == pInner) && (pInner != NULL))
            {
                const meta::port_t *meta = pInner->metadata();
                float v = pInner->value();
                if (bReverse)
                    v = meta->max - v;

                if (pOuter->value() != v)
                {
                    pOuter->set_value(v);
                    pOuter->notify_all();
                }
            }
            else if ((port == pOuter) && (pOuter != NULL))
            {
                const meta::port_t *meta = pOuter->metadata();
                float v = pOuter->value();
                if (bReverse)
                    v = meta->max - v;

                if (pInner->value() != v)
                {
                    pInner->set_value(v);
                    pInner->notify_all();
                }
            }
        }

        //-------------------------------------------------------------------------
        // Main class methods

        room_builder_ui::room_builder_ui(const meta::plugin_t *metadata):
            ui::Module(metadata),
            sPresets(this),
            sAbsorption(this, false),
            sTransparency(this, true),
            sDispersion(this, false),
            sDiffuse(this, false)
        {
            nSelected       = -1;
        }

        room_builder_ui::~room_builder_ui()
        {
        }

        status_t room_builder_ui::init(ui::IWrapper *wrapper, tk::Display *dpy)
        {
            status_t res = ui::Module::init(wrapper, dpy);
            if (res != STATUS_OK)
                return res;

            const meta::port_t *meta = meta::room_builder_metadata::kvt_ports;

            // Create object identifier port
            CtlListPort *kvt_list = new CtlListPort(this, meta++);
            if (kvt_list == NULL)
                return STATUS_NO_MEM;
            pWrapper->bind_custom_port(kvt_list);
            pWrapper->kvt_subscribe(kvt_list);

            CtlFloatPort *p;

    #define BIND_KVT_PORT(pattern, field)    \
            p = new CtlFloatPort(this, pattern, meta++); \
            if (p == NULL) \
                return STATUS_NO_MEM; \
            kvt_list->add_port(p); \
            pWrapper->bind_custom_port(p); \
            pWrapper->kvt_subscribe(p);

            BIND_KVT_PORT("enabled", fEnabled);
            BIND_KVT_PORT("position/x", sPos.x);
            BIND_KVT_PORT("position/y", sPos.y);
            BIND_KVT_PORT("position/z", sPos.z);
            BIND_KVT_PORT("rotation/yaw", fYaw);
            BIND_KVT_PORT("rotation/pitch", fPitch);
            BIND_KVT_PORT("rotation/roll", fRoll);
            BIND_KVT_PORT("scale/x", fSizeX);
            BIND_KVT_PORT("scale/y", fSizeY);
            BIND_KVT_PORT("scale/z", fSizeZ);
            BIND_KVT_PORT("color/hue", fHue);
            BIND_KVT_PORT("material/absorption/outer", fAbsorption[0]);
            BIND_KVT_PORT("material/absorption/inner", fAbsorption[1]);
            BIND_KVT_PORT("material/absorption/link", lnkAbsorption);
            BIND_KVT_PORT("material/dispersion/outer", fDispersion[0]);
            BIND_KVT_PORT("material/dispersion/inner", fDispersion[1]);
            BIND_KVT_PORT("material/dispersion/link", lnkDispersion);
            BIND_KVT_PORT("material/diffusion/outer", fDiffusion[0]);
            BIND_KVT_PORT("material/diffusion/inner", fDiffusion[1]);
            BIND_KVT_PORT("material/diffusion/link", lnkDiffusion);
            BIND_KVT_PORT("material/transparency/outer", fTransparency[1]);
            BIND_KVT_PORT("material/transparency/inner", fTransparency[1]);
            BIND_KVT_PORT("material/transparency/link", lnkTransparency);
            BIND_KVT_PORT("material/sound_speed", fSndSpeed);

            sAbsorption.init(UI_KVT_PORT_PREFIX "oabs", UI_KVT_PORT_PREFIX "iabs", UI_KVT_PORT_PREFIX "labs");
            sTransparency.init(UI_KVT_PORT_PREFIX "otransp", UI_KVT_PORT_PREFIX "itransp", UI_KVT_PORT_PREFIX "ltransp");
            sDispersion.init(UI_KVT_PORT_PREFIX "odisp", UI_KVT_PORT_PREFIX "idisp", UI_KVT_PORT_PREFIX "ldisp");
            sDiffuse.init(UI_KVT_PORT_PREFIX "odiff", UI_KVT_PORT_PREFIX "idiff", UI_KVT_PORT_PREFIX "ldiff");

            return STATUS_OK;
        }

        status_t room_builder_ui::post_init()
        {
            status_t res = ui::Module::post_init();
            if (res == STATUS_OK)
                sPresets.init("mpreset", UI_KVT_PORT_PREFIX "oid", UI_KVT_PORT_PREFIX "speed", UI_KVT_PORT_PREFIX "oabs");
            return res;
        }

        void room_builder_ui::kvt_cleanup_objects(core::KVTStorage *kvt, size_t objects)
        {
            core::KVTIterator *it = kvt->enum_branch("/scene/object");
            while (it->next() == STATUS_OK)
            {
                const char *id = it->id();
                if (id == NULL)
                    continue;

                // Must be a pure object identifier
                errno = 0;
                char *endptr;
                long value = ::strtol(id, &endptr, 10);
                if ((errno != 0) || (size_t(endptr - id) != size_t(::strlen(id))))
                    continue;

                // Remove the object
                if ((value < 0) || (value >= ssize_t(objects)))
                {
                    lsp_trace("Removing KVT parameters from %s", it->name());
                    it->remove_branch();
                }
            }
        }

    } // namespace plugui
} // namespace lsp


