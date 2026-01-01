{
  "targets": [
    {
      "target_name": "webcodecs",
      "sources": [
        "src/addon.cpp",
        "src/shared/codec_registry.cpp",
        "src/ImageDecoder.cpp",
        "src/ImageTrackList.cpp",
        "src/ImageTrack.cpp",
        "src/AudioDecoder.cpp",
        "src/VideoDecoder.cpp",
        "src/AudioEncoder.cpp",
        "src/VideoEncoder.cpp",
        "src/EncodedAudioChunk.cpp",
        "src/EncodedVideoChunk.cpp",
        "src/AudioData.cpp",
        "src/VideoFrame.cpp",
        "src/VideoColorSpace.cpp"
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
              "MACOSX_DEPLOYMENT_TARGET": "10.15"
            },
            "libraries": [
              "<(ffmpeg_libdir)/libavformat.a",
              "<(ffmpeg_libdir)/libavcodec.a",
              "<(ffmpeg_libdir)/libswscale.a",
              "<(ffmpeg_libdir)/libavutil.a",
              "-framework CoreFoundation",
              "-framework CoreMedia",
              "-framework CoreVideo",
              "-framework VideoToolbox",
              "-framework Security",
              "-framework AudioToolbox",
              "-lbz2",
              "-lz",
              "-liconv"
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
              "<(ffmpeg_libdir)/libavformat.a",
              "<(ffmpeg_libdir)/libavcodec.a",
              "<(ffmpeg_libdir)/libswscale.a",
              "<(ffmpeg_libdir)/libavutil.a",
              "-lpthread",
              "-ldl",
              "-lz",
              "-llzma"
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
