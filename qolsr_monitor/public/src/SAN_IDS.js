//
// SAN_IDS.js
//
// instantiates all the helper classes, sets up the particle system + renderer
// and maintains the canvas/editor splitview
//
  var network;
  var gsrc = ''; 
(function(){
  trace = arbor.etc.trace
  objmerge = arbor.etc.objmerge
  objcopy = arbor.etc.objcopy
  var parse = Parseur().parse
  var an_event = Event_parser().eparse
  var attackers = []

  function getTime() {
    return moment().format('h:mm:ss');
  }

  function addFormatIncident(attacker) {
    $('#code').prepend("<div style='margin-top: 8px;' class='row'><div class='col-md-3'><span class='label label-default'>" + getTime() + "</span></div><div class='col-md-5'><span class='label label-warning'>Malformed Packet</span></div><div class='col-md-4'><span class='label label-warning'>" + attacker + "</span></div></div>")
  }

  function addTopoIncident(attacker,type) {
    $('#code').prepend("<div style='margin-top: 8px;' class='row'><div class='col-md-3'><span class='label label-default'>" + getTime() + "</span></div><div class='col-md-5'><span class='label label-danger'>"+type+"</span></div><div class='col-md-4'><span class='label label-danger'>" + attacker + "</span></div></div>")
  }

  function addOrange(attacker) {
    a_node = (attacker.split("."))[3];

    network.nodes[a_node].color = "red";
    sys.merge(network);

/*
          show_event = "Declaration of link that does not exist\n"
                       + "Spoofed link between:\n    " + o + "\nand: \n    " + n
                       + "\nAttacker:\n    " + e.attributes.attacker
                       + "\n************************************\n"
*/
  }
  function getNodeIndex (ipaddress) {
    switch(ipaddress){
      case "10.43.46.30":
        return 1;
      case "10.82.140.154":
        return 2;
      case "10.43.123.57":
        return 3;
      case "10.47.243.99":
        return 4;
      case "10.47.252.163":
        return 5;
      case "10.40.29.120":
        return 6;
      default:
        console.log("Node doesn't exist: " + ipaddress);
        return 0;
    }
  }
  function addLink(attacker, neighbor) {
    n_node = getNodeIndex(neighbor);
    a_node = getNodeIndex(attacker);
    // console.log(network);
    // console.log(n_node + ' ---- ' + a_node);
    network.edges[a_node][n_node] = {directed: true, color: "red"};
    network.nodes[a_node].color = "red";
    sys.merge(network);

/*
          show_event = "Declaration of link that does not exist\n"
                       + "Spoofed link between:\n    " + o + "\nand: \n    " + n
                       + "\nAttacker:\n    " + e.attributes.attacker
                       + "\n************************************\n"
*/
  }

  var SANIDS = function(elt){
    var dom = $(elt)

    sys = arbor.ParticleSystem(2600, 512, 0.5)
    sys.renderer = Renderer("#viewport") // our newly created renderer will have its .init() method called shortly by sys...
    sys.screenPadding(20)
    
    var _ed = dom.find('#editor')
    var _code = dom.find('#code')
    var _canvas = dom.find('#viewport').get(0)
    var _grabber = dom.find('#grabber')
    
    var _updateTimeout = null
    var _current = null // will be the id of the doc if it's been saved before
    var _editing = false // whether to undim the Save menu and prevent navigating away
    var _failures = null
    
    var that = {
      dashboard:Dashboard("#dashboard", sys),
      io:IO("#editor .io"),
      init:function(){
      console.log("That is init()!"+gsrc)
        $(window).resize(that.resize)
        that.resize()
        that.updateLayout(Math.max(1, $(window).width()-340))

        //_code.keydown(that.typing)
        _grabber.bind('mousedown', that.grabbed)
        that.getDoc({id:"SAN_IDS"})
        $(that.io).bind('get', that.getDoc)
        $(that.io).bind('clear', that.newDoc)
        return that
      },
      
      getDoc:function(e){
        console.log("getDoc is called");
        $.getJSON('/library/'+e.id+'.json', function(doc){
            console.log("getDoc: doc "+doc);
          // update the system parameters
          if (doc.sys){
            sys.parameters(doc.sys)
            that.dashboard.update()
          }

          // modify the graph in the particle system
          //_code.val(doc.src)
          gsrc = doc.src;
          that.updateGraph()
          that.resize()
          _editing = false
        })
        
      },

      newDoc:function(){
        var lorem = "; some example nodes\nhello {color:red, label:HELLO}\nworld {color:orange}\n\n; some edges\nhello -> world {color:yellow}\nfoo -> bar {weight:5}\nbar -> baz {weight:2}"
        
        //_code.val(lorem).focus()
        $.address.value("")
        that.updateGraph()
        that.resize()
        _editing = false
      },

      updateGraph:function(e){
        //var src_txt = _code.val()
        console.log("updateGraph");
        var src_txt = gsrc;
        network = parse(src_txt)

//TODO: subscribe to events
        //var eve = an_event('{"v":"1.0","ts":1407945088840,"type":"verdict","data":{"id":"LinkSpoofing.verdict","value":false,"instance_id":"192.168.200.255"},"attributes":{"orig":"192.168.200.2","neighbor":"192.168.200.5","type":10,"attacker":"192.168.200.2"}}', network)
/*
        var eve = an_event(
'{"v":"1.0","ts":1407945088840,"type":"verdict","data":{"id":"PacketFormat.verdict","value":false,"instance_id":"192.168.200.255"},"attributes":{"attacker":"192.168.200.2"}}'
, network)
*/

//TODO: append incomming events
        //_code.val(eve)
        //addLink("192.168.200.4", "192.168.200.5", network);

        sys.merge(network)
        _updateTimeout = null

      },
      
      resize:function(){        
        var w = $(window).width() - 40
        var x = w - _ed.width()
        that.updateLayout(x)
        sys.renderer.redraw()
      },
      
      updateLayout:function(split){
        var w = dom.width()
        var h = _grabber.height()
        var split = split || _grabber.offset().left
        var splitW = _grabber.width()
        _grabber.css('left',split)

        var edW = w - split
        var edH = h
        _ed.css({width:edW, height:edH})
        if (split > w-20) _ed.hide()
        else _ed.show()

        var canvW = split - splitW
        var canvH = h
        _canvas.width = canvW
        _canvas.height = canvH
        sys.screenSize(canvW, canvH)
                
        _code.css({height:h-20,  width:edW-4, marginLeft:2})
      },
      
      grabbed:function(e){
        $(window).bind('mousemove', that.dragged)
        $(window).bind('mouseup', that.released)
        return false
      },
      dragged:function(e){
        var w = dom.width()
        that.updateLayout(Math.max(10, Math.min(e.pageX-10, w)) )
        sys.renderer.redraw()
        return false
      },
      released:function(e){
        $(window).unbind('mousemove', that.dragged)
        return false
      },
      typing:function(e){
        var c = e.keyCode
        if ($.inArray(c, [37, 38, 39, 40, 16])>=0){
          return
        }
        
        if (!_editing){
          $.address.value("")
        }
        _editing = true
        
        if (_updateTimeout) clearTimeout(_updateTimeout)
        _updateTimeout = setTimeout(that.updateGraph, 900)
      }
    }
    console.log("That is created!")
    return that.init()    
  }

      
  $(document).ready(function(){
    console.log("Connected ???");
    var mcp = SANIDS("#SAN_IDS")    

    var socket = new io.connect('localhost:8080/');
    console.log(socket);
    var failNb = 0;

    socket.on('connect', function() {
      console.log("Connected");
    });
    //addFormatIncident('192.168.1.1');  
    //addFormatIncident('192.168.1.1');  
    socket.on('message', function(message){
      var msg = JSON.parse(message);
      console.log(message);
      if( msg.channel === 'PacketFormat.verdict' && msg.data.data.value === false) {
        addFormatIncident(msg.data.attributes.attacker);
        id = (msg.data.attributes.attacker).replace(/\./g, '_');
        val = parseInt($('#' + id).text()) + 1;
        $('#' + id).text(val);
        addOrange(msg.data.attributes.attacker);
      } else if( msg.channel === 'LinkSpoofing.verdict' && msg.data.data.value === false) {
        addTopoIncident(msg.data.attributes.attacker,"LinkSpoofing");  
        addLink(msg.data.attributes.attacker, msg.data.attributes.neighbor);
        id = (msg.data.attributes.attacker).replace(/\./g, '_');
        val = parseInt($('#' + id).text()) + 1;
        $('#' + id).text(val);
      }else if(msg.channel === 'Linkqualityspoofing.verdict' && msg.data.data.value === false){
        addTopoIncident(msg.data.attributes.attacker,"Linkqualityspoofing");  
        addLink(msg.data.attributes.attacker, msg.data.attributes.neighbor);
        id = (msg.data.attributes.attacker).replace(/\./g, '_');
        val = parseInt($('#' + id).text()) + 1;
        $('#' + id).text(val);
      }
      else if ( msg.channel === 'context.cpu' ) {
        cpuusage = 100 - msg.data.data.idle;
        $('#cpu').text(cpuusage.toFixed(2) + ' %');
      }
    }) ;

    socket.on('disconnect', function() {
      console.log('disconnected');
    });

  })

  
})()
