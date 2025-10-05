idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES simpleOTA nvs_flash
                       EMBED_FILES "../data/index.html"
                                   "../data/main.css" 
                                   "../data/main.js"
                       WHOLE_ARCHIVE)