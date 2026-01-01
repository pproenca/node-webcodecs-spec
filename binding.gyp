{
  "targets": [
    {
      "target_name": "webcodecs",
      "sources": [
        "src/addon.cpp",
        "src/generated/ImageDecoder.cpp",
        "src/generated/ImageTrackList.cpp",
        "src/generated/ImageTrack.cpp",
        "src/generated/AudioDecoder.cpp",
        "src/generated/VideoDecoder.cpp",
        "src/generated/AudioEncoder.cpp",
        "src/generated/VideoEncoder.cpp",
        "src/generated/EncodedAudioChunk.cpp",
        "src/generated/EncodedVideoChunk.cpp",
        "src/generated/AudioData.cpp",
        "src/generated/VideoFrame.cpp",
        "src/generated/VideoColorSpace.cpp"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "<!(echo ${FFMPEG_ROOT:-/usr/local/ffmpeg})/include"
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
            "xcode_settings": {
              "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
              "CLANG_CXX_LANGUAGE_STANDARD": "c++17",
              "MACOSX_DEPLOYMENT_TARGET": "10.15"
            },
            "libraries": [
              "<!(echo ${FFMPEG_ROOT:-/usr/local/ffmpeg})/lib/libavformat.a",
              "<!(echo ${FFMPEG_ROOT:-/usr/local/ffmpeg})/lib/libavcodec.a",
              "<!(echo ${FFMPEG_ROOT:-/usr/local/ffmpeg})/lib/libswscale.a",
              "<!(echo ${FFMPEG_ROOT:-/usr/local/ffmpeg})/lib/libavutil.a",
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
            "cflags_cc": [
              "-std=c++17",
              "-fPIC"
            ],
            "libraries": [
              "<!(echo ${FFMPEG_ROOT:-/usr/local/ffmpeg})/lib/libavformat.a",
              "<!(echo ${FFMPEG_ROOT:-/usr/local/ffmpeg})/lib/libavcodec.a",
              "<!(echo ${FFMPEG_ROOT:-/usr/local/ffmpeg})/lib/libswscale.a",
              "<!(echo ${FFMPEG_ROOT:-/usr/local/ffmpeg})/lib/libavutil.a",
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
            "msvs_settings": {
              "VCCLCompilerTool": {
                "ExceptionHandling": 1,
                "AdditionalOptions": [
                  "/std:c++17"
                ]
              }
            },
            "libraries": [
              "<!(echo %FFMPEG_ROOT%)/lib/avformat.lib",
              "<!(echo %FFMPEG_ROOT%)/lib/avcodec.lib",
              "<!(echo %FFMPEG_ROOT%)/lib/swscale.lib",
              "<!(echo %FFMPEG_ROOT%)/lib/avutil.lib"
            ]
          }
        ]
      ]
    }
  ]
}
