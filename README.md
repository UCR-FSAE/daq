# daq  
Note: was working in terminal, not sure how uv stuff will interact with PlatformIO  
Link to Foxglove API can be found [here](https://foxglove-sdk-api-docs.pages.dev/python/api/#foxglove.start_server).

# Directory of added files
|-- scripts  
|  |-  foxglove_test.py  
|  |-  lora_simulator.py  
|  |  
|-  .python-version  
|-  pyproject.toml  
|-  uv.lock  

Currently, only tested Foxglove implementation through its desktop app on MacOS. 
Link to download page can be found [here](https://foxglove.dev/download?utm_term=download%20foxglove&utm_campaign=PF-Search_Brand_Sign-Ups&utm_source=adwords&utm_medium=ppc&utm_content=812651119371&hsa_acc=7660688999&hsa_cam=23476036068&hsa_grp=198975492882&hsa_ad=812651119371&hsa_src=g&hsa_tgt=kwd-2336421552850&hsa_kw=download%20foxglove&hsa_mt=e&hsa_net=adwords&hsa_ver=3&gad_source=1&gad_campaignid=23476036068&gbraid=0AAAAABaqO8MFJj3cy3hFAfJNW1bmQdILn&gclid=Cj0KCQjwlsfWBhD4ARIsACL7p1EJDbjT1alJdsYRKZ200tVI3MJoHHi0nx4smSUXI4hzPDLuLf7p3jkaAgzZEALw_wcB).

If want to run files in script folder, run `uv run python3 scripts/<file name>` in root. In the Foxglove app, click on __Open Connection__ and select ws://127.0.0.1:8765. ws://127.0.0.1:8765 is the default WebSocket server address and port used by Foxglove.
- foxglove_test.py simulates data to Foxglove
- lora_simulator.py simulates behavior of radio link to Foxglove

# Visualizations
seq  
uptime_ms  
timestamp_ms  
timestamp_utc  
motor_temp_c  
inverter_temp_c  
highest_subpack_temp_c  
speed_kph  
motor_rpm  
can_current_a  
rms_current_a  
pack_voltage_v  
rms_voltage_v  
power_w  
soc_pct  
