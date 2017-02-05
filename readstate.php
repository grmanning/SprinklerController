<!DOCTYPE HTML>
<html lang="en-US">
<head>
<meta charset="UTF-8">
<title>Read State</title>


</head>
<body>
<?php
// String $actualData[];
// String $today;
// String $currentState;
// String $actualLine;
// date_default_timezone_set('Australia/Canberra');
// 	$currentState = "";
// 	if (!empty($_POST["controllerState"])) {
// 		$actualState = htmlspecialchars($_POST["controllerState"]);
// 		$fh = fopen("actualstate.txt","w");
// 		$actualStateDate = date("Y-m-d");
// 		$actualStateTime = date("H:i:s");
// 		$actualIRTemp = "NoReading";
// 		if (!empty($_POST["irTemp"])) { 
// 			$actualIRTemp = htmlspecialchars($_POST["irTemp"]);
// 		}
// 		fwrite($fh,"$actualState,$actualStateDate,$actualStateTime,$actualIRTemp\n");
// 		fclose($fh);
// 	}
	
	$actualLine = NULL;
	$fh = fopen("actualstate.txt","r") or $actualLine = "NotReadYet,0000-00-00,00:00:00,0.0";
	if ($actualLine == NULL) {  $actualLine = fgets($fh); }
	$actualData = explode(",",$actualLine);
	$actualState = $actualData[0];
	$actualStateDate = $actualData[1];
	$actualStateTime = $actualData[2];
	$actualIRTemp = $actualData[3];
	fclose($fh);
	 
	 	
    $firsttime = 0;
    $fh = fopen("mirrorstate.txt","r") 
    	or $firsttime = 1;
    
    if ($firsttime) {
    	echo "Nostate>0000-00-00>00:00:00>$actualState>$actualStateDate>$actualStateTime>$actualIRTemp";
    	 }
	else {
		$line = fgets($fh);
		$mirrorData = explode(",",$line);
		$mirrorState = $mirrorData[0];
		$mirrorStateDate = $mirrorData[1];
		$mirrorStateTime = $mirrorData[2];
		echo "Last controller setting was $mirrorState on $mirrorStateDate at $mirrorStateTime </br> Current controller state is: $actualState last read on $actualStateDate at $actualStateTime </br> Last temperature reading: $actualIRTemp";
	 }
	 
	

	 
    ?>

<!-- 
<form action = "" method = "post">
<textarea name = "controllerState"></textarea>
<textarea name = "irTemp"></textarea>
<input type = "submit" name = "Send"/>
</form>
 -->


</body>
</html>