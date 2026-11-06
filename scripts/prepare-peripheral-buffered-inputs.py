#!/usr/bin/env python3
"""Prepare a transfer through a real blocking guest read, without line redraw.

This only writes reviewable input files; it never executes an emulator or injects
input. Use in a new explicitly bound continuation, after the HAL control succeeds.
Each phase still needs its own current source SHA/count and native admission.
"""
import argparse
import base64
import gzip
import hashlib
import json
import pathlib
import shlex
import subprocess


def digest(data):
    return hashlib.sha256(data).hexdigest()


def marker(name):
    return "printf '\\nREVERSE_%s RC_0\\n' "+shlex.quote(name.removeprefix("REVERSE_"))


def root_command(body, response):
    return "su 0 sh -c "+shlex.quote(body)+"; r=$?; printf '\\nREVERSE_%s RC_%s\\n' "+shlex.quote(response.removeprefix("REVERSE_"))+" \"$r\"\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build_directory", type=pathlib.Path)
    parser.add_argument("output_directory", type=pathlib.Path)
    args = parser.parse_args()
    build, output = args.build_directory, args.output_directory
    manifest = json.loads((build/"build-manifest.json").read_text())
    executable = (build/"cuttlefish-peripheral-peer-aarch64").read_bytes()
    compressed = (build/"peer-aarch64.gz").read_bytes()
    assert (build/"execution-commit.txt").read_text().strip() == "e11a4b86cdb9f79f8d553f67a09bf74afddd9cbe"
    assert digest(executable) == manifest["ELF_SHA256"] == "20110632e38c9fa662d0b813250ca16da0481251be0daacbc43dee84479a7efb"
    assert digest(compressed) == manifest["gzip_SHA256"] == "2646fcd488bccea5b2e3ef9ea1b3e6b0218410dad393e17886fe7c3b6c6c1632"
    assert gzip.decompress(compressed) == executable
    encoded = base64.b64encode(compressed).decode("ascii")
    chunks = [encoded[i:i+2900] for i in range(0,len(encoded),2900)]
    assert len(chunks) == 2
    base = "/data/local/tmp/reverseandroid-peer-e11a"
    data_file, elf_file, log_file = base+".b64", base+".elf", base+".log"
    capture = "set -e; umask 077; test ! -e "+data_file+"; test ! -L "+data_file+"; "
    for index in range(2):
        capture += marker("REVERSE_PEER_RX_"+str(index))+"; IFS= read -r data || exit 21; "
        capture += 'printf %s "$data" '+("> " if index == 0 else ">> ")+data_file+"; "
        capture += "h=$(sha256sum "+data_file+'); test "${h%% *}" = '+digest("".join(chunks[:index+1]).encode())+"; "
        capture += marker("REVERSE_PEER_DATA_"+str(index))+"; "
    capture += "true"
    activation = "set -e; umask 077; h=$(sha256sum "+data_file+'); test "${h%% *}" = '+digest(encoded.encode())+"; "
    activation += "test ! -e "+elf_file+"; test ! -L "+elf_file+"; base64 -d "+data_file+" | gzip -dc > "+elf_file+"; "
    activation += "sha256sum "+elf_file+"; h=$(sha256sum "+elf_file+'); test "${h%% *}" = '+manifest["ELF_SHA256"]+"; "
    activation += "chmod 700 "+elf_file+"; "+elf_file+" --guest > "+log_file+" 2>&1 < /dev/null & p=$!; "
    activation += 'ready=0; i=0; while test "$i" -lt 20; do if grep -q "^REVERSE_PERIPHERAL_PEER_READY$" '+log_file+'; then ready=1; break; fi; kill -0 "$p" || break; sleep 1; i=$((i+1)); done; cat '+log_file+'; test "$ready" = 1'
    inspection = "id; cat "+log_file+"; ls -lZ /dev/hvc10 /dev/hvc18 /dev/hvc19; service check android.hardware.oemlock.IOemLock/default; service check android.hardware.sensors.ISensors/default; ps -A -o PID,UID,NAME,ARGS | grep -E \"oemlock|sensors|reverseandroid-peer\"; true"
    commands = [root_command(capture,"REVERSE_PEER_CAPTURE"),chunks[0]+"\n",chunks[1]+"\n",
                root_command(activation,"REVERSE_PEER_ACTIVATION"),root_command(inspection,"REVERSE_PEER_INSPECT")]
    responses = ["REVERSE_PEER_RX_0","REVERSE_PEER_DATA_0","REVERSE_PEER_DATA_1","REVERSE_PEER_ACTIVATION","REVERSE_PEER_INSPECT"]
    # Only actual shell commands are syntax checked. Base64 phases are data for
    # the already waiting root read builtin, not commands for a shell parser.
    for text in [capture,activation,inspection,commands[0],commands[3],commands[4]]:
        subprocess.run(["bash","-n"],input=text,text=True,check=True)
    output.mkdir(parents=True,exist_ok=False)
    phases=[]
    for index,(command,response) in enumerate(zip(commands,responses)):
        data=command.encode("ascii");assert 0 < len(data) <= 4096
        path=output/f"input-{index}.txt";path.write_bytes(data)
        phases.append({"input_file":path.name,"input_bytes":len(data),"input_sha256":digest(data),
                       "response_marker":response,"failure_markers":["REVERSE_PEER_CAPTURE"],
                       "data_for_waiting_guest_read":index in (1,2)})
    result={"schema":"reverseandroid-buffered-UART-transfer-proposal-v1","ELF_SHA256":manifest["ELF_SHA256"],
            "gzip_SHA256":manifest["gzip_SHA256"],"phase_count":len(phases),"phases":phases,
            "requires_new_source_bound_input_plan_per_phase":True,"source_bound_or_executed":False,
            "requires_successful_prior_original_HAL_control":True,"modifies_original_interpreter_or_codecs":False}
    (output/"proposal.json").write_text(json.dumps(result,indent=2)+"\n")
    print(json.dumps({"phases":len(phases),"input_bytes":[p["input_bytes"] for p in phases],
                      "actual_guest_transfer_test_passed":False,"emulator_execution_started":False}))


if __name__ == "__main__":
    main()
