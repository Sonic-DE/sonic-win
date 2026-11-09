# SPDX-FileCopyrightText: 2026 Sebastian Sauer <dipesh@gmx.de>
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

kwin_add_builtin_effect(dwellclicker
    main.cpp
    dwellclickereffect.cpp
)

target_link_libraries(dwellclicker PRIVATE
    kwin
)
