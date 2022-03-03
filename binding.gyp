{
  "targets": [
    {
      "target_name": "ddb",
      "sources": [
        "src/av.cc",
        "src/error.cc",
        "src/nodejs.cc"
      ],
      "libraries": [
        "-lavcodec",
        "-lavfilter",
        "-lavutil",
        "-lavformat",
        "-lswscale"
      ],
      "cflags_cc": [
        "-std=c++17",
        "-Wno-deprecated-declarations"
      ],
      "msvs_settings": {
        "VCCLCompilerTool": {
          "AdditionalOptions": [ "-std:c++17" ],
        }
      }
    }
  ]
}
