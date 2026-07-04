# Sanitizer wiring. Selected via preset: -DCOURSE_SANITIZE=address,undefined
# or -DCOURSE_SANITIZE=thread. Applied globally so assignment code AND test
# code are instrumented.
set(COURSE_SANITIZE "" CACHE STRING
    "Comma-separated sanitizers to enable (address,undefined | thread)")

if(COURSE_SANITIZE)
    add_compile_options(-fsanitize=${COURSE_SANITIZE} -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=${COURSE_SANITIZE})
endif()
