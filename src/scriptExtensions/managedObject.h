/**
 * =============================================================================
 * cs_script_extensions
 * Copyright (C) 2026 liondoge
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#include "v8.h"
#include "scriptExtensions/userMessageInfo.h"

template<typename T>
class ManagedObject 
{
public:
    ManagedObject(v8::Isolate* isolate, v8::Local<v8::Object> obj, T* d)
        : data(d), persistent(isolate, obj)
    {
        persistent.SetWeak(
            this,
            &ManagedObject::WeakCallback,
            v8::WeakCallbackType::kParameter
        );
    }

    T* GetData() const {
        return data;
    }

protected:
    static void WeakCallback(const v8::WeakCallbackInfo<ManagedObject>& info) {
        ManagedObject* self = info.GetParameter();
        self->persistent.Reset();
        delete self->data;
        delete self;
    }

    T* data;
    v8::Persistent<v8::Object> persistent;
};