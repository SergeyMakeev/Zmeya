// The MIT License (MIT)
//
// Copyright (c) 2021-2025 Sergey Makeev
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
#pragma once

/*

**Zmeya umbrella header**

The library is split into focused headers (containers, serialization, hashing). Include this file to
get the same surface area as a single monolithic header.

*/

#include "ZmeyaConfig.h"
#include "ZmeyaHash.h"
#include "ZmeyaTypes.h"
#include "ZmeyaPointer.h"
#include "ZmeyaString.h"
#include "ZmeyaArray.h"
#include "ZmeyaHashAdapters.h"
#include "ZmeyaHashSet.h"
#include "ZmeyaHashMap.h"

#ifdef ZMEYA_ENABLE_SERIALIZE_SUPPORT
#include "ZmeyaSerializeFoundation.h"
#include "ZmeyaBuilderBase.h"
#include "ZmeyaBuilderAssignImpl.h"
#include "ZmeyaBuilder.h"
#include "ZmeyaBlobWriter.h"
#include "ZmeyaSerializeApi.h"
#endif

#include "ZmeyaStdHash.h"
