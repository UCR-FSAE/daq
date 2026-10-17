Note: was working in terminal, not sure how uv stuff will interact with PlatformIO

# Directory of added files
|--scripts <-- currently here :)
|  |- foxglove_test.py
|  |- lora_simulator.py
|  |
|- .python-version
|- pyproject.toml
|- uv.lock

Currently, only tested Foxglove implementation through its desktop app on MacOS. 
Link to it can be found [here](https://foxglove.dev/download?utm_term=download%20foxglove&utm_campaign=PF-Search_Brand_Sign-Ups&utm_source=adwords&utm_medium=ppc&utm_content=812651119371&hsa_acc=7660688999&hsa_cam=23476036068&hsa_grp=198975492882&hsa_ad=812651119371&hsa_src=g&hsa_tgt=kwd-2336421552850&hsa_kw=download%20foxglove&hsa_mt=e&hsa_net=adwords&hsa_ver=3&gad_source=1&gad_campaignid=23476036068&gbraid=0AAAAABaqO8MFJj3cy3hFAfJNW1bmQdILn&gclid=Cj0KCQjwlsfWBhD4ARIsACL7p1EJDbjT1alJdsYRKZ200tVI3MJoHHi0nx4smSUXI4hzPDLuLf7p3jkaAgzZEALw_wcB)

If want to run files in script folder, run `uv run python3 scripts/<file name>` in root
In the Foxglove app, click on __Open Connection__ and select ws://127.0.0.1:8765. 
ws://127.0.0.1:8765 is the default WebSocket server address and port used by Foxglove.
- foxglove_test.py simulates data to Foxglove
- lora_simulator.py simulates behavior of radio link to Foxglove
