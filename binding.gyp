{
  "targets": [
    {
      "target_name": "webcodecs",
      "sources": [
        "src/addon.cpp",
        "src/shared/codec_registry.cpp",
        "src/image_decoder.cpp",
        "src/image_track_list.cpp",
        "src/image_track.cpp",
        "src/audio_decoder.cpp",
        "src/video_decoder.cpp",
        "src/audio_encoder.cpp",
        "src/video_encoder.cpp",
        "src/encoded_audio_chunk.cpp",
        "src/encoded_video_chunk.cpp",
        "src/audio_data.cpp",
        "src/video_frame.cpp",
        "src/video_color_space.cpp"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "<!@(pkg-config --cflags-only-I libavcodec 2>/dev/null | sed 's/-I//g' || echo '')"
      ],
      "defines": [
        "NAPI_VERSION=8",
        "NAPI_CPP_EXCEPTIONS"
      ],
      "cflags!": [
        "-fno-exceptions"
      ],
      "cflags_cc!": [
        "-fno-exceptions"
      ],
      "cflags_cc": [
        "-std=c++17"
      ],
      "conditions": [
        [
          "OS=='mac'",
          {
            "variables": {
              "ffmpeg_libdir": "<!(pkg-config --variable=libdir libavcodec 2>/dev/null || brew --prefix ffmpeg 2>/dev/null | xargs -I{} echo {}/lib || echo /usr/local/lib)"
            },
            "xcode_settings": {
              "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
              "CLANG_CXX_LANGUAGE_STANDARD": "c++17",
              "MACOSX_DEPLOYMENT_TARGET": "15.0"
            },
            "libraries": [
              "<!@(pkg-config --libs libavformat libavcodec libswscale libavutil 2>/dev/null || echo '-lavformat -lavcodec -lswscale -lavutil')",
              "-framework CoreFoundation",
              "-framework CoreMedia",
              "-framework CoreVideo",
              "-framework VideoToolbox",
              "-framework Security",
              "-framework AudioToolbox"
            ]
          }
        ],
        [
          "OS=='linux'",
          {
            "variables": {
              "ffmpeg_libdir": "<!(pkg-config --variable=libdir libavcodec 2>/dev/null || echo /usr/local/lib)"
            },
            "cflags_cc": [
              "-std=c++17",
              "-fPIC"
            ],
            "libraries": [
              "<!@(pkg-config --libs libavformat libavcodec libswscale libavutil 2>/dev/null || echo '-lavformat -lavcodec -lswscale -lavutil')",
              "-lpthread",
              "-ldl"
            ]
          }
        ],
        [
          "OS=='win'",
          {
            "variables": {
              "ffmpeg_root": "<!(echo %FFMPEG_ROOT%)"
            },
            "include_dirs": [
              "<(ffmpeg_root)/include"
            ],
            "msvs_settings": {
              "VCCLCompilerTool": {
                "ExceptionHandling": 1,
                "AdditionalOptions": [
                  "/std:c++17"
                ]
              }
            },
            "libraries": [
              "<(ffmpeg_root)/lib/avformat.lib",
              "<(ffmpeg_root)/lib/avcodec.lib",
              "<(ffmpeg_root)/lib/swscale.lib",
              "<(ffmpeg_root)/lib/avutil.lib"
            ]
          }
        ]
      ]
    }
  ]
}
