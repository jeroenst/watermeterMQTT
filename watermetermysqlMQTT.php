#!/usr/bin/php
<?php

require(realpath(dirname(__FILE__))."/../phpMQTT/phpMQTT.php");


$server = "127.0.0.1";     // change if necessary
$port = 1883;                     // change if necessary
$username = "";                   // set your username
$password = "";                   // set your password
$client_id = "watermetermysqlMQTT"; // make sure this is unique for connecting to sever - you could use uniqid()
$topicprefix = 'home/watermeter/';

$settings = array(
"mysqlserver" => "localhost",
"mysqlusername" => "casaan",
"mysqlpassword" => "gWdtGxQDnq6NhSeG",
"mysqldatabase" => "casaan");


$mqttdata = array();

$mqtt = new phpMQTT($server, $port, $client_id);

$lastgasdatetime = "";

if(!$mqtt->connect(true, NULL, $username, $password)) {
	exit(1);
}

echo "Connected to mqtt server...\n";
$topics[$topicprefix.'m3'] = array("qos" => 0, "function" => "newvalue");
$mqtt->subscribe($topics, 0);

while($mqtt->proc()){
}


$mqtt->close();

function newvalue($topic, $msg)
{
	echo "$topic = $msg\n";
	static $mqttdata;
	global $mqtt;
	global $topicprefix;
	global $settings;
	static $lastgasdatetime;
	
       $newdata["total"]["m3"] = $msg;
	
		echo ("Calculating new water values...\n"); 
		
	        $mysqli = mysqli_connect($settings["mysqlserver"],$settings["mysqlusername"],$settings["mysqlpassword"],$settings["mysqldatabase"]);

	        if (($mysqli) && (!$mysqli->connect_errno))
	        {
	        	// Write watervalues to database
                        $mysqli->query("INSERT INTO `watermeter` (m3) VALUES (".$msg.");");

                        // Read values from database
                        if ($result = $mysqli->query("SELECT * FROM `watermeter` WHERE timestamp >= CURDATE() ORDER BY timestamp ASC LIMIT 1"))
                        {
                                $row = $result->fetch_object();
                                //var_dump ($row);
                                $newdata["today"]["m3"] = number_format($newdata["total"]["m3"] - $row->m3,3);
                                $mqtt->publish($topicprefix."today/m3", $newdata["today"]["m3"], 0,1);
                        }
                        else
                        {
                                echo "error reading water values from database ".$mysqli->error."\n";
                        }



                        // Read values from database
                        if ($result = $mysqli->query("SELECT * FROM `watermeter` WHERE DATE(timestamp) = CURDATE() - INTERVAL 1 DAY ORDER BY timestamp ASC LIMIT 1"))
                        {
                                $row = $result->fetch_object();
                                if ($row)
                                {
                                        $newdata['yesterday']['m3'] = round($newdata["total"]["m3"] - $row->m3 - ["today"]["m3"],3);
                                        $mqtt->publish($topicprefix."yesterday/m3", $newdata["yesterday"]["m3"],0,1);
                                }
                                else $mqtt->publish($topicprefix."yesterday/m3", "",0,1);   
                        }
                        else
                        {
                                echo "error reading water values from database ".$mysqli->error."\n";
                        }


                        // Read values from database
                        if ($result = $mysqli->query("SELECT * FROM `watermeter` WHERE DATE(timestamp) = CURDATE() - INTERVAL 1 MONTH ORDER BY timestamp ASC LIMIT 1"))
                        {
                                $row = $result->fetch_object();
                                $newdata['month']['m3'] = round($newdata["total"]["m3"]  - $row->m3, 3);
                                $mqtt->publish($topicprefix."month/m3", $newdata["month"]["m3"],0,1);
                        }
                        else
                        {
                                echo "error reading water values from database ".$mysqli->error."\n";
                        }

                        // Read values from database
                        if ($result = $mysqli->query("SELECT * FROM `watermeter` WHERE timestamp >= DATE_FORMAT(NOW() ,'%Y-01-01') ORDER BY timestamp ASC LIMIT 1"))
                        {
                                $row = $result->fetch_object();
                                var_dump ($row);
                                $newdata['year']['m3'] = round($newdata["total"]["m3"]  - $row->m3, 3);
                                $mqtt->publish($topicprefix."year/m3", $newdata["year"]["m3"],0,1);
                        }
                        else
                        {
                                echo "error reading water values from database ".$mysqli->error."\n";
                        }
	        
			$mysqli->close();
	        }
		else
		{
			echo "Connection to myqsl failed!\n";
		}
		
		

		
	
}
