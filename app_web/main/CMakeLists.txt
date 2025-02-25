idf_component_register(SRCS "main.c" "wifi_manager/src/wifi_manager.c" "server/src/server.c"
                    INCLUDE_DIRS "." "wifi_manager/include" "server/include"
                    EMBED_FILES webpage/app.css webpage/app.js webpage/index.html )
