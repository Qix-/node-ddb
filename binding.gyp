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
      ]
    }
  ]
}
