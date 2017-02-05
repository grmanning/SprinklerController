<!DOCTYPE HTML>
<html lang="en-US">
<head>
<meta charset="UTF-8">
<title>Controller</title>

<style type="text/css">
   body {
		font-size: 3.5em;
		background-color: #00DDDD;
	}
	input[type=radio] {
		border: 0px;
		width: 10%;
		height: 4em;
	}
	input[type=submit] {
		padding:5px 15px; 
		border:0 none;
		cursor:pointer;
		-webkit-border-radius: 10px;
		border-radius: 10px; 
		font-size:0.9em;
		width: 7em
	}
   .history {
		font-size: 0.6em;
		background-color: #00DDDD;
	}
	.refresh {
		color: green;
		background-color: #DDDDDD;
	}
	.undo {
		color: red;
		background-color: #DDDDDD;
	}
    </style>

</head>
<body>
<?php
date_default_timezone_set('Australia/Canberra');
	$name = "";
	if (isset($_POST['Standby'])) {
		$name = "Standby";
	}
	if (isset($_POST['Protect'])) {
		$name = "Protect";
	}
	if (isset($_POST['AllOn'])) {
		$name = "AllOn";
	}
	if (isset($_POST['Alert'])) {
		$name = "Alert";
	}
	if ($name != "") {
		$today = date("Y-m-d");
   	 	$fh = fopen("controlstate.txt","w");
    	fwrite($fh,"$name $today\n");
    	fclose($fh);
    		$today = date("Y-m-d,H:i:s");
    	$fh = fopen("../mirrorstate.txt","w");
    	fwrite($fh,"$name,$today\n");
    	fclose($fh);
    	$lh = fopen("controllog.txt","a");
		fwrite($lh,"ADD $name $today\n");
		fclose($lh);
	 }

    ?>
<table width="100%" border="0" cellspacing="0" cellpadding="0">
<tbody>
<tr bgcolor="#FFFF11">
<td 1 class="pageName" id="form" nowrap="wrap" align="middle" valign="middle" bgcolor="#00DDDD" width="50%">
	Sprinkler <br>Remote <br>Control
	</td>
<td 2 class="pageName" id="logo" nowrap="nowrap" align="middle" valign="middle" bgcolor="#00DDDD" width="50%">

</td>
</tr>
<tr bgcolor="#FFFF11" >
<td 1 class="pageName" id="form" nowrap="nowrap" valign="top" bgcolor="#00DDDD" width="100%" colspan="2">
<form action="controller.php" method="POST">
<fieldset>
	<legend><font size="6.5em">State change commands:</font></legend>
	<table width="100%" border="0" cellspacing="12" cellpadding="0"><tr  width="50%">	
		<td><input type="submit" name="Standby" value="Standby"></td>
		<td><input type="submit" name="Protect" value="Protect"></td>
	</tr>
	<tr width="50%">
		<td><input type="submit" name="AllOn" value="AllOn"></td>
		<td><input type="submit" name="Alert" value="Alert"></td>
	</tr>
	</table>
</td>
</tr>
</tbody>
</table>

<table width="100%" border="0" cellspacing="0" cellpadding="0">
<tbody>
<tr>
<td 4 valign="top" width="50%">
<form action="controller.php" method="POST">
<fieldset>
	<legend><font size="6.5em">History:</font></legend>
<input type="submit" name="refresh" value="Refresh" class="refresh">
<br><br>
<input type="submit" name="undo" value="Undo" class="undo">
</fieldset>
</form>
</td>
<td 3>
    <div class = "history">
<?php
    $firsttime = 0;
    $fh = fopen("controlstate.txt","r") 
    	or $firsttime = 1;
    
    if ($firsttime) {
    	echo "No state set";
    	 }
	else {
		// echo "<font color='blue'>Totals to date:</font><br />";
		$i = 0;
	 	while(!feof($fh)) {
	 		$line[$i] = fgets($fh);
	 		$indx = substr($line[$i],0,strpos($line[$i]," "));
	 		$count[$indx]++;
	 		$i = $i + 1;
	 		}
// 	 	 foreach ($count as $suspect => $bottles) {
// 			if ($suspect !== 0) {
// 				echo "$suspect:$bottles	";
// 			}
//	 	}
	 echo "<hr><font color='blue'>Current Command:</font><br />";	
	 $t = $i - 2;
	 while ($t >= 0) {
	 	$date = substr($line[$t],strpos($line[$t]," ") + 1);
	 	$name = substr($line[$t],0,strpos($line[$t]," "));
	 	echo "$date $name<br />";
	 	$t = $t - 1;
	 	}
	 }
	 
	$actualLine = NULL;
	$fh = fopen("../actualstate.txt","r") or $actualLine = "NotReadYet,0000-00-00,00:00:00,0.0";
	if ($actualLine == NULL) {  $actualLine = fgets($fh); }
	$actualData = explode(",",$actualLine);
	$actualState = $actualData[0];
	$actualStateDate = $actualData[1];
	$actualStateTime = $actualData[2];
	$actualIRTemp = $actualData[3];
	fclose($fh);	 
	 
	 echo "<hr><font color='blue'>Current Actual State:</font><br />";	
	 echo "$actualStateDate $actualState $actualStateTime</br>";
	 echo "Current temperature: $actualIRTemp";
	 
	 
    ?>
    </div>
</td>

</tr>
</tbody>
</table>

</body>
</html>