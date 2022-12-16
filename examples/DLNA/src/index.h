/*
 *  index.h
 *
 *  Created on: 13.12.2022
 *  Updated on:
 *      Author: Wolle
 *
 *  ESP32 - DLNA
 *
 */

#ifndef INDEX_H_
#define INDEX_H_

#include "Arduino.h"

// file in raw data format for PROGMEM

const char index_html[] PROGMEM = R"=====(
<!DOCTYPE HTML>
<html>
<head>
    <title>ESP32 - DLNA</title>
    <style type="text/css">           /* optimized with csstidy */
        html {  /* This is the groundplane */
            font-family : serif;
            height : 100%;
            font-size: 16px;
            color : DarkSlateGray;
            background-color : navy;
            margin : 0;
            padding : 0;
        }
        #content {
            min-height : 540px;
            min-width : 725px;
            overflow : hidden;
            background-color : lightskyblue;
            margin : 0;
            padding : 5px;
        }
        #tab-content1 {
            margin : 20px;
        }
        .boxstyle {
            height : 36px;
            padding-top : 0;
            padding-left : 15px;
            padding-bottom : 0;
            background-color: white;
            font-size : 16px;
            line-height : normal;
            border-color: black;
            border-style: solid;
            border-width: thin;
            border-radius : 5px;
        }
        #BODY { display:block; }
    </style>
</head>
<script>
    // global variables and functions

// ---- websocket section------------------------

var socket = undefined
var host = location.hostname
var tm

function ping() {
    if (socket.readyState == 1) { // reayState 'open'
        socket.send("ping")
        console.log("send ping")
        tm = setTimeout(function () {
            console.log('The connection to the ESP32 is interrupted! Please reload the page!')
        }, 10000)
    }
}

function connect() {
    socket = new WebSocket('ws://'+window.location.hostname+':81/');

    socket.onopen = function () {
        console.log("Websocket connected")
        socket.send('DLNA_getServer')
        setInterval(ping, 20000)
    };

    socket.onclose = function (e) {
        console.log(e)
        console.log('Socket is closed. Reconnect will be attempted in 1 second.', e)
        socket = null
        setTimeout(function () {
          connect()
        }, 1000)
    }

    socket.onerror = function (err) {
        console.log(err)
    }

    socket.onmessage = function(event) {
        var socketMsg = event.data
        var n   = socketMsg.indexOf('=')
        var msg = ''
        var val = ''
        if (n >= 0) {
            var msg  = socketMsg.substring(0, n)
            var val  = socketMsg.substring(n + 1)
//          console.log("para ",msg, " val ",val)
        }
        else {
          msg = socketMsg
        }

        switch(msg) {
            case "pong":            clearTimeout(tm)
                                    break
            case "DLNA_Names":      showServer(val)
                                    break
            case "Level0":          show_DLNA_Content(val, 0)
                                    break
            case "Level1":          show_DLNA_Content(val, 1)
                                    break
            case "Level2":          show_DLNA_Content(val, 2)
                                    break
            case "Level3":          show_DLNA_Content(val, 3)
                                    break
            default:                console.log('unknown message', msg, val)
        }
    }
}
// ---- end websocket section------------------------

document.addEventListener('readystatechange', event => {
  if (event.target.readyState === 'interactive') { // same as:  document.addEventListener('DOMContentLoaded'...
    // same as  jQuery.ready
    console.log('All HTML DOM elements are accessible')
    // document.getElementById('dialog').style.display = 'none' // hide the div (its only a template)

  }
  if (event.target.readyState === 'complete') {
    console.log('Now external resources are loaded too, like css,src etc... ')
    connect();  // establish websocket connection
  }
})

function showServer(val){
    console.log(val)
    var select = document.getElementById('server')
    select.options.length = 0;
    var server = val.split(",")
    for (i = -1; i < (server.length); i++) {
        opt = document.createElement('OPTION')
        if(i == -1){
          opt.value = ""
          opt.text =  "Select a DLNA Server here"
        }
        else{
          console.log(server[i])
          opt.value = server[i]
          opt.text =  server[i]
        }
        select.add(opt)
    }
}

function show_DLNA_Content(val, level){
    var select
    if(level == 0) select = document.getElementById('level1')
    if(level == 1) select = document.getElementById('level2')
    if(level == 2) select = document.getElementById('level3')
    if(level == 3) select = document.getElementById('level4')
    content =JSON.parse(val)
    //console.log(ct[1].name)
    select.options.length = 0;
    for (i = -1; i < (content.length); i++) {
        opt = document.createElement('OPTION')
        if(i == -1){
            opt.value = ""
            opt.text =  "Select level " + level.toString()
        }
        else{
            var n
            var c
            if(content[i].isDir == true){
                n = content[i].name + ' <DIR>';
                c = 'D=' + content[i].id // is directory
            }
            else{
                n = content[i].name + ' ' + content[i].size;
                c = 'F=' + content[i].id // is file
            }
            opt.value = c
            opt.text  = n
        }
        select.add(opt)
    }
}

function selectserver (presctrl) { // preset, select a server
    socket.send('DLNA_getContent0=' + presctrl.value)
    select = document.getElementById('level1'); select.options.length = 0; // clear next level
    select = document.getElementById('level2'); select.options.length = 0;
    select = document.getElementById('level3'); select.options.length = 0;
    select = document.getElementById('level4'); select.options.length = 0;
    console.log('DLNA_getContent0=' + presctrl.value)
}

function select_l0 (presctrl) { // preset, select root
    socket.send('DLNA_getContent1=' + presctrl.value)
    select = document.getElementById('level2'); select.options.length = 0; // clear next level
    select = document.getElementById('level3'); select.options.length = 0;
    select = document.getElementById('level4'); select.options.length = 0;
    console.log('DLNA_getContent1=' + presctrl.value)
}

function select_l1 (presctrl) { // preset, select level 1
    socket.send('DLNA_getContent2=' + presctrl.value)
    select = document.getElementById('level3'); select.options.length = 0;
    select = document.getElementById('level4'); select.options.length = 0;
    console.log('DLNA_getContent2=' + presctrl.value)
}

function select_l2 (presctrl) { // preset, select level 2
    socket.send('DLNA_getContent3=' + presctrl.value)
    select = document.getElementById('level4'); select.options.length = 0;
    console.log('DLNA_getContent3=' + presctrl.value)
 }

 function select_l3 (presctrl) { // preset, select level 3
    socket.send('DLNA_getContent4=' + presctrl.value)
    console.log('DLNA_getContent4=' + presctrl.value)
 }

</script>
<body id="BODY">
    <!--==============================================================================================-->
<div id="content">
<div id="content1">
    <div style="font-size: 50px; text-align: center; flex: 1;">
      ESP32 - DLNA
    </div>
    <div style="display: flex;">
        <div style="flex: 0 0 calc(100% - 0px);">
            <select class="boxstyle" style="width: 100%;" onchange="selectserver(this)" id="server">
                <option value="-1">Select a DLNA Server here</option>
            </select>
            <select class="boxstyle" style="width: 100%; margin-top: 5px;" onchange="select_l0(this)" id="level1">
                 <option value="-1"> </option>
            </select>
            <select class="boxstyle" style="width: 100%; margin-top: 5px;" onchange="select_l1(this)" id="level2">
                <option value="-1"> </option>
            </select>
            <select class="boxstyle" style="width: 100%; margin-top: 5px;" onchange="select_l2(this)" id="level3">
                <option value="-1"> </option>
            </select>
            <select class="boxstyle" style="width: 100%; margin-top: 5px;" onchange="select_l3(this)" id="level4">
                <option value="-1"> </option>
            </select>
        </div>
    </div>
    <hr>
</div>
</div>
    <!--==============================================================================================-->
</body>
</html>

)=====";
#endif /* INDEX_H_ */