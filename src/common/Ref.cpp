/*
 * Copyright (c) 2014 - 2020 Christian Schoenebeck
 *
 * http://www.linuxsampler.org
 *
 * This file is part of LinuxSampler and released under the same terms.
 * See README file for details.
 */

#include "Ref.h"

namespace LinuxSampler {

    #if LS_REF_ASSERT_MODE
    std::set<void*> _allRefPtrs;
    #endif
    #if LS_REF_ASSERT_MODE && LS_REF_ATOMIC
    Mutex _allRefPtrsMutex;
    #endif

} // namespace LinuxSampler
