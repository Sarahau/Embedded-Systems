// Imports
const functions = require('@google-cloud/functions-framework');
const {Firestore} = require('@google-cloud/firestore');

// Function variables
const gfsCollectionName = "m5_data";

//////////////////////////////////////////////////////////////
// Function that is triggered when HTTP request is made
//////////////////////////////////////////////////////////////
functions.http('average', async (req, res) => {
	// Get the request details header 
	const strHeaderM5 = req.get('Req-Details');
	
	// Attempt to convert the string to a JSON object
	let reqDetails;
	try {
		// Converting String to JSON object
		reqDetails = JSON.parse(strHeaderM5);

		// Check that JSON object has essential components
		if (!reqDetails.hasOwnProperty("userId") || 
            !reqDetails.hasOwnProperty("timeDuration") ||
			!reqDetails.hasOwnProperty("dataType")) {
				let message = 'Could NOT parse JSON details in header';
				console.error(message);
				res.status(400).send("Malformed request.");
			}
	} catch(e) {
		let message = 'Could NOT parse JSON details from header.';
		console.error(message);
		res.status(400).send("Malformed request.");
	}

	// Parse request info from the HTTP custom header 
	const userId = reqDetails.userId;
	const timeDuration = reqDetails.timeDuration;
	const dataType = reqDetails.dataType;

	// time range to calculate the average from
    const timeCurr = Date.now() / 1000; // UTC epoch in seconds
    const timeStart = timeCurr - (timeDuration); // start time in UTC epoch in seconds

	console.log("timecurr: " + timeCurr);
	console.log("timestart: " + timeStart);
	
	// Firestore client
	const firestore = new Firestore(); 

	// Get the details from Google Firestore
	try {

		// returning variables
		// a. Data type (e.g., ax, temp, rHum) (variable already exists)
		let stringTimeCurr = toPrettyTimeString(timeCurr); // b. Range of actual data (e.g., 4/14/2025 1:20:05pm – 4/14/2025 1:20:32pm)
		let stringTimeStart = toPrettyTimeString(timeStart); // b. Range of actual data (e.g., 4/14/2025 1:20:05pm – 4/14/2025 1:20:32pm)

		var numOccurences = 0; // c. Number of data points found within the timeDuration
		var rateOfDataCollection = 0; // d. Rate of data collection (data points per second...just (c)/(b)) will be overridden later

		var maxValue = -10000.0; // extremely low number so that the max will get overridden 
		var minValue = 10000.0; // extremely high number so that the min will get overridden 
		
		var totalValues = 0.0; 
		var averagedValue = 0.0;

        if (userId == "raz" || userId == "taz") {
            const userRef = firestore.collection(gfsCollectionName).doc("users").collection(userId);
            const snapshots = await userRef.where('otherDetails.timeCaptured', '>=', timeStart).get();
            
            if (snapshots.empty) {
                console.log('No matching documents.');
				res.status(204).send("No matching documents");
                return;
            }

            snapshots.forEach(doc => {
				numOccurences++; // increment counter
				let snapshotValue = -1;
				
				// check for the data type requested
				if (dataType == "ax" || dataType == "ay" || dataType == "az") {
					snapshotValue = doc.data()['m5Details'][dataType];
				} else if (dataType == "rHum" || dataType == "temp") {
					snapshotValue = doc.data()['shtDetails'][dataType];
				} else if (dataType == "als" || dataType == "prox" || dataType == "rwl") {
					snapshotValue = doc.data()['vcnlDetails'][dataType];
				}

				if (snapshotValue < minValue) {
					minValue = snapshotValue; // replace the minValue with the new smaller value
				} 
				if (snapshotValue > maxValue) {
					maxValue = snapshotValue; // replace the maxValue with the new larger value
				}

				totalValues += snapshotValue; // add to the total for the averaging
            });

			averagedValue = totalValues / numOccurences;
			rateOfDataCollection = numOccurences / timeDuration;
        } else if (userId == "all") {
			// go through Tabby's data
			const tazUserRef = firestore.collection(gfsCollectionName).doc("users").collection("taz"); 
            const tazSnapshots = await tazUserRef.where('otherDetails.timeCaptured', '>=', timeStart).get();
            
            if (tazSnapshots.empty) {
                console.log('No matching documents.');
				res.status(204).send("No matching documents");
                return;
            }

            tazSnapshots.forEach(doc => {
				numOccurences++; // increment counter
				let snapshotValue = -1;
				
				// check for the data type requested
				if (dataType == "ax" || dataType == "ay" || dataType == "az") {
					snapshotValue = doc.data()['m5Details'][dataType];
				} else if (dataType == "rHum" || dataType == "temp") {
					snapshotValue = doc.data()['shtDetails'][dataType];
				} else if (dataType == "als" || dataType == "prox" || dataType == "rwl") {
					snapshotValue = doc.data()['vcnlDetails'][dataType];
				}

				if (snapshotValue < minValue) {
					minValue = snapshotValue; // replace the minValue with the new smaller value
				} 
				if (snapshotValue > maxValue) {
					maxValue = snapshotValue; // replace the maxValue with the new larger value
				}

				totalValues += snapshotValue; // add to the total for the averaging
            });

			// go through Sarah's data
			const razUserRef = firestore.collection(gfsCollectionName).doc("users").collection("raz"); 
            const razSnapshots = await razUserRef.where('otherDetails.timeCaptured', '>=', timeStart).get();
            
            if (razSnapshots.empty) {
                console.log('No matching documents.');
				res.status(204).send("No matching documents");
                return;
            }

            razSnapshots.forEach(doc => {
				numOccurences++; // increment counter
				let snapshotValue = -1;
				
				// check for the data type requested
				if (dataType == "ax" || dataType == "ay" || dataType == "az") {
					snapshotValue = doc.data()['m5Details'][dataType];
				} else if (dataType == "rHum" || dataType == "temp") {
					snapshotValue = doc.data()['shtDetails'][dataType];
				} else if (dataType == "als" || dataType == "prox" || dataType == "rwl") {
					snapshotValue = doc.data()['vcnlDetails'][dataType];
				}

				if (snapshotValue < minValue) {
					minValue = snapshotValue; // replace the minValue with the new smaller value
				} 
				if (snapshotValue > maxValue) {
					maxValue = snapshotValue; // replace the maxValue with the new larger value
				}

				totalValues += snapshotValue; // add to the total for the averaging
            });

			averagedValue = totalValues / numOccurences;
			rateOfDataCollection = numOccurences / timeDuration;
		} else {
			res.status(404).message("couldn't find userID");
			return;
		}

		let responseObj = {'dataType' : dataType, 'averageData' : averagedValue, 
		'minData' : minValue, 'maxData' : maxValue, 'timeRange' : `${stringTimeStart} - ${stringTimeCurr}`, 
		'numDataPoints' : numOccurences, 'rateDataCollect' : rateOfDataCollection};

		res.status(200).send(responseObj);

	} catch (e) {
		let eMessage = `GFS ERROR: Could NOT get details from ${gfsCollectionName}: ${e}`;
		console.log(eMessage)
	}

	// If made it here, we did not have a successful run; return a generic error message to the client.
	res.status(400).send("There was an exception that occurred during runtime.");

});

function toPrettyTimeString(seconds) {
	let d = new Date(0);
	d.setUTCSeconds(seconds);
	let stringTime = d.toLocaleString('en-US', { timeZone: 'PST' });
	return stringTime;
}

/*
///////////////////////////////////////////////////////////////
// "Pretty" JSON Model Example
///////////////////////////////////////////////////////////////
{
	"userId": "tester",
	"timeDuration": "6000",
	"dataType": "als"
}

///////////////////////////////////////////////////////////////
// "Minimized" JSON Model Example
///////////////////////////////////////////////////////////////
{ "userId": "tester", "timeDuration": "6000", "dataType": "als" }
*/