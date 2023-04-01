
/* ****************************************************************
 * Copyright Stanford University 1998,99 - All Rights Reserved
 ******************************************************************

 * Permission to use, copy, modify, and distribute this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appears in all copies
 * and that both the copyright notice, this permission notice, and
 * the following disclaimer appear in supporting documentation, and
 * that the name of Stanford University, not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.
 *
 * STANFORD UNIVERSITY, DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS. IN NO EVENT SHALL STANFORD UNIVERSITY BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * The Sim Web Site : http://klamath.stanford.edu/tools/SIM
 * The SIM Mailing List: sim-simulator@lists.stanford.edu

 * Send mail to the above email address with "subscribe sim-simulator" in
 * the body of the message.
 *
 */

#include <string.h>
#include "sim.h"
#include "algorithm.h"
#include "drr.h"
#include "scheduleStats.h"
#include "miscfns.h"

/*static void printState();*/
static int selectRequest();
static int selectGrant();
static int selectAccept();
static void createScheduleState();
static InputSchedulerState *createInScheduler();
static OutputSchedulerState *createOutScheduler();

/*  Determines switch configuration 					*/
/*  Configuration is filled into aSwitch->fabric.Xbar_matrix array 	*/

typedef struct {
  struct Element *next;
  struct Element *prev;
  int *inputAcceptPointer;
  int *outputGrantPointer;
  int *connection;
} State;

void
drr(action, aSwitch, argc, argv)
  SwitchAction action;
Switch *aSwitch;
int argc;
char **argv;
{
  SchedulerState 			*scheduleState;
  InputSchedulerState 	*inputSchedule;
  OutputSchedulerState	*outputSchedule;
  int input, switchOutput, fabricOutput, numFabricOutputs,reqOutput;
  Cell *aCell;
  struct List *fifo;

  if(debug_algorithm)
    printf("Algorithm 'drr()' called by switch %d\n", aSwitch->switchNumber);

  switch(action) {
  case SCHEDULING_USAGE:
    fprintf(stderr, "Options for \"drr\" scheduling algorithm:\n");
    fprintf(stderr, "    -n number_of_iterations. Default: 1\n");
    break;
  case SCHEDULING_INIT:
    if(debug_algorithm) printf("	SCHEDULING_INIT\n");

    if( aSwitch->scheduler.schedulingState == NULL )
      {
	extern int opterr;
	extern int optopt;
	extern int optind;
	extern char *optarg;
	int c;


	/* Allocate scheduler state for this switch */
	createScheduleState(aSwitch);
	scheduleState = (SchedulerState *) aSwitch->scheduler.schedulingState;
	scheduleState->numIterations=1;

	opterr=0;
	optind=1;
	while( (c = getopt(argc, argv, "n:")) != EOF)
	  switch (c)
	    {
	    case 'n':
	      scheduleState->numIterations = atoi(optarg);
	      break;
	    case '?':
	      fprintf(stderr, "-------------------------------\n");
	      fprintf(stderr, "drr: Unrecognized option -%c\n", optopt);
	      drr(SCHEDULING_USAGE);
	      fprintf(stderr, "--------------------------------\n");
	      exit(1);
	    default:
	      break;
	    }


	/* Reset and initialize scheduling stats for this switch. */
	scheduleCellStats( SCHEDULE_CELL_STATS_INIT, aSwitch );

	printf("numIterations %d\n", scheduleState->numIterations);
      }

    break;

  case SCHEDULING_EXEC:
    {
      int iteration;
      int numConnectionsThisIteration;

      if(debug_algorithm) printf("	SCHEDULING_EXEC\n");

      scheduleState=(SchedulerState *)aSwitch->scheduler.schedulingState;
      numFabricOutputs = aSwitch->numOutputs *
	aSwitch->fabric.Xbar_numOutputLines;

      /*************** INITIALIZE ***************/
      /* Set up inputs (grants and accepts) */
	 
	 //clearing request buffer from input scheduler for every time slot
	 int i,j;
	  for(i=0;i<numFabricOutputs;i++){
		outputSchedule = scheduleState->outputSched[i];
		for(j=0;j<aSwitch->numInputs;j++){
			outputSchedule->request[j]=0;
		}
		printf("]\n");
	  }
	  
      for(input=0; input<aSwitch->numInputs; input++)
	{
	  inputSchedule = scheduleState->inputSched[input];
	  inputSchedule->accept = NONE;

	  for(fabricOutput=0;fabricOutput<numFabricOutputs;fabricOutput++)
	    inputSchedule->grant[fabricOutput] = 0;
	}

      /* Set up outputs (accepts) */
      for(fabricOutput=0;fabricOutput<numFabricOutputs;fabricOutput++)
	{
	  outputSchedule = scheduleState->outputSched[fabricOutput];
	  outputSchedule->accept=NONE;
	  outputSchedule->grant=NONE;
	}


      /* Check for newly arrived cells at head of input queues. */
      for(input=0; input<aSwitch->numInputs; input++)
	for(switchOutput=0; switchOutput<aSwitch->numOutputs;
	    switchOutput++)
	  {
	    fifo = aSwitch->inputBuffer[input]->fifo[switchOutput];
	    if( fifo->number )
	      {
		aCell = fifo->head->Object;
		scheduleCellStats(SCHEDULE_CELL_STATS_HEAD_ARRIVAL,
				  aSwitch, aCell);
	      }
	  }

      /* Calculate and update synchronization stats */
      countSynch(aSwitch);
      /***************  END INITIALIZE ***************/

      /*****************************/
      /*****   Main Loop  **********/
      /*****************************/
      for(iteration=0;iteration<scheduleState->numIterations;iteration++)
	{
	  numConnectionsThisIteration = 0;

      for(input=0; input<aSwitch->numInputs; input++) {
        /* accept output for each input  */
        reqOutput = selectRequest(aSwitch, input);
        if( reqOutput >= 0 ) {
          inputSchedule = scheduleState->inputSched[input];
          outputSchedule= scheduleState->outputSched[reqOutput];

          inputSchedule->request = reqOutput;
          outputSchedule->request[input] = 1;
        }
      }
	  
	  int i,j;
	  for(i=0;i<numFabricOutputs;i++){
		outputSchedule = scheduleState->outputSched[i];
		printf("Request array of output %d : [\t",i);
		for(j=0;j<aSwitch->numInputs;j++){
			printf("%d\t",outputSchedule->request[j]);
		}
		printf("]\n");
	  }

	  for(fabricOutput=0;fabricOutput<numFabricOutputs;fabricOutput++)
	    {

	      /* grant an input for this output line */
	      if(debug_algorithm)
		printf("Selecting grant for output %d\n", fabricOutput);
	      input = selectGrant(aSwitch, fabricOutput);
	      if(debug_algorithm)
		printf("Selected input %d\n", input);
	      if( input != NONE )
		{
		  inputSchedule = scheduleState->inputSched[input];
		  outputSchedule=scheduleState->outputSched[fabricOutput];
		  inputSchedule->grant[fabricOutput] = 1;
		  outputSchedule->grant = input;
		}
	    }


	  for(input=0; input<aSwitch->numInputs; input++)
	    {
	      /* accept output for each input  */
	      fabricOutput = selectAccept(aSwitch, input);
	      if( fabricOutput >= 0 ) {
		numConnectionsThisIteration++;
		inputSchedule = scheduleState->inputSched[input];
		outputSchedule=scheduleState->outputSched[fabricOutput];
		inputSchedule->accept = fabricOutput;
		outputSchedule->accept = input;

	      }
	    }
	  /* If no connections were made during this iteration, */
	  /* the algorithm has converged and so there is no point */
	  /* in continuing. */
	  if (!numConnectionsThisIteration)
	    break;
	}
      scheduleStats(SCHEDULE_STATS_NUM_ITERATIONS, aSwitch, &iteration);

      for(fabricOutput=0; fabricOutput<numFabricOutputs; fabricOutput++)
	{
	  input = scheduleState->outputSched[fabricOutput]->accept;
	  aSwitch->fabric.Xbar_matrix[fabricOutput].input = input;
	  if(input!=NONE)
	    aSwitch->fabric.Xbar_matrix[fabricOutput].cell = (Cell *)
	      aSwitch->inputBuffer[input]->fifo[fabricOutput]->head->Object;
	}
      break;
    }

  case SCHEDULING_REPORT_STATS:
    scheduleStats( SCHEDULE_STATS_PRINT_ALL, aSwitch );
    scheduleCellStats( SCHEDULE_CELL_STATS_PRINT_ALL, aSwitch );
    break;
  case SCHEDULING_INIT_STATS:
    scheduleStats( SCHEDULE_STATS_INIT, aSwitch );
    scheduleCellStats( SCHEDULE_CELL_STATS_INIT, aSwitch );
    break;
  case SCHEDULING_REPORT_STATE:
    scheduleState=(SchedulerState *) aSwitch->scheduler.schedulingState;

    numFabricOutputs = aSwitch->numOutputs *
      aSwitch->fabric.Xbar_numOutputLines;

    printf("Output: ");
    for(fabricOutput=0; fabricOutput<numFabricOutputs; fabricOutput++)
      printf("%2d ", fabricOutput);
    printf("Input: ");
    for(input=0; input<aSwitch->numInputs; input++)
      printf("%2d ", input);
    printf("\n        ");
    for(fabricOutput=0; fabricOutput<numFabricOutputs; fabricOutput++)
      printf("%2d ",
	     ((scheduleState->outputSched[fabricOutput]->last_accepted_grant) + 1)%aSwitch->numInputs);
    printf("       ");
    for(input=0; input<aSwitch->numInputs; input++)
      printf("%2d ",
	     ((scheduleState->inputSched[input]->last_accept)
	      + 1)%numFabricOutputs);
    printf("\n");
    break;

  default:
    break;
  }

  if(debug_algorithm)
    printf("Algorithm 'drr()' completed for switch %d\n", aSwitch->switchNumber);
}

/***********************************************************************/
static int
selectRequest(aSwitch, input)
Switch *aSwitch;
int input;
/* Function declartion above is pre ANSI-C style which is same as:
 * static int selectRequest(Switch *aSwitch, int input)
 *
 * Function takes two parameters:
 * Switch *aSwitch:     Pointer to Switch
 * int input:           input for which we are selecting request
 *
 */
{
  // The request selected by this function will be stored in this variable
  int requested_output;

  // Scheduler State of switch copied to local variable
  SchedulerState *scheduleState = (SchedulerState *)aSwitch->scheduler.schedulingState;

  // Input scheduler state for current input copied to local variable
  InputSchedulerState *inputSchedule = scheduleState->inputSched[input];

  // Create local copies of input buffer and cell
  InputBuffer  *inputBuffer;
  Cell *aCell;

  int increment;
  int request_pointer;

  /**************************************/
  // Iterate over all outputs
  for(increment=1; increment<=aSwitch->numOutputs; increment++){

      // increment request pointer circular
      request_pointer = (inputSchedule->last_request + increment)%(aSwitch->numOutputs);

      // requested_output is the output that request_pointer is pointing at
      requested_output = inputSchedule->RR_requests[request_pointer];

      // copy input buffer of current input to local variable
      inputBuffer = aSwitch->inputBuffer[input];

      /* if (
       *    1: Current input's VOQ for requested_output has packets
       *    AND
       *    2: The reqested output has not already granted an input
       * )
       */
      if(( inputBuffer->fifo[requested_output]->number > 0 )
          && ( scheduleState->outputSched[requested_output]->grant == NONE )){

          // move current request to last request
          inputSchedule->last_request = requested_output;
          if(debug_algorithm)
              printf("Input %d requesting to output %d\n", input, requested_output);

          // The request has been succesfully selected
          return(requested_output);
        }
    }

  if(debug_algorithm)
    printf("Input %d requested no output. None granted.\n", input);

  // No request selected
  return(NONE);
}




/***********************************************************************/
static int
selectGrant(aSwitch, output)
Switch *aSwitch;
int output;
/* Function declartion above is pre ANSI-C style which is same as:
 * static int selectGrant(Switch *aSwitch, int output)
 *
 * Function takes two parameters:
 * Switch *aSwitch:     Pointer to Switch
 * int output:          output for which we are selecting grant
 *
 */
 {
  // The grant selected by this function will be stored in this variable
  int granted_input;

  // Scheduler State of switch copied to local variable
  SchedulerState *scheduleState = (SchedulerState *)aSwitch->scheduler.schedulingState;

  // Output scheduler state for current input copied to local variable
  OutputSchedulerState *outputSchedule=scheduleState->outputSched[output];

  // Create local copies of input buffer and cell
  InputBuffer  *inputBuffer;
  Cell *aCell;

  int increment;
  int grant_pointer;

  int switchOutput=output/aSwitch->fabric.Xbar_numOutputLines;

  /* Check to see if output has already been accepted by an input from
       an earlier iteration */
  if( outputSchedule->accept != NONE ){
      if(debug_algorithm)
        printf("Output %d already accepted by input %d\n", output, outputSchedule->accept);
      return(NONE);
    }

  /**************************************/

  // Iterate over all inputs
  for(increment=1; increment<=aSwitch->numInputs; increment++)
    {
      // increment grant pointer circular
      grant_pointer = (outputSchedule->last_accepted_grant + increment)%(aSwitch->numInputs);

      // granted_input is the input that grant_pointer is pointing at
      granted_input = outputSchedule->RR_grants[grant_pointer];

      // copy input buffer of current input to local variable
      inputBuffer = aSwitch->inputBuffer[granted_input];

      /* if (
       *    1: input pointed by grant_pointer has request for current output
       *    AND
       *    2: The granted_input has not already accepted a grant
       * )
       */
      if(( outputSchedule->request[grant_pointer] == 1 )
        && ( scheduleState->inputSched[granted_input]->accept == NONE )){
			if(debug_algorithm)
				printf("o/p Sched -> req[grant_ptr]: %d\n",outputSchedule->request[grant_pointer]);

        /* STATS */
        /* Mark grantTime for cell at head of line */
        /* aCell = (Cell *) inputBuffer->fifo[switchOutput]->head->Object;
        scheduleCellStats( SCHEDULE_CELL_STATS_UPDATE_GRANT,
        aSwitch, aCell); */
        /* END STATS */

        outputSchedule->last_accepted_grant = granted_input;
        
		if(debug_algorithm)
          printf("Output %d granting to input %d\n", output, granted_input);
        return(granted_input);
        }
    }

  if(debug_algorithm)
    printf("No inputs requested output %d. None granted.\n", output);

  return(NONE);

}

static int
selectAccept(aSwitch, input)
  Switch *aSwitch;
int input;
{
  /*
    Choose output to accept from schedule
    */
  SchedulerState	*scheduleState=(SchedulerState *)aSwitch->scheduler.schedulingState;
  InputSchedulerState *inputSchedule=scheduleState->inputSched[input];


  Cell *aCell;
  int output, switchOutput, numFabricOutputs;

  int increment, sched_output;


  numFabricOutputs = aSwitch->numOutputs*aSwitch->fabric.Xbar_numOutputLines;

  if( inputSchedule->accept != NONE )
    return( NONE );

	/*	How many outputs granted to this input? */
	/*
	numGrants=0;
	for(output=0; output<numFabricOutputs; output++)
	{
		if( inputSchedule->grant[output] == 1 )
			numGrants++;
	}
	if( numGrants > 1 )
		printf("%d outputs granted to input %d\n", numGrants, input);
	*/

  for( increment=1; increment<=numFabricOutputs; increment++ )
    {
      sched_output = (inputSchedule->last_accept + increment)
	%(numFabricOutputs);
      output = inputSchedule->RR_accepts[sched_output];
      if( inputSchedule->grant[output] == 1 )
	{
	  inputSchedule->last_accept = output;

	  /* STATS */
	  /* Mark cell at head of line as accepted */
	  switchOutput = output / aSwitch->fabric.Xbar_numOutputLines;
	  aCell = (Cell *)
	    aSwitch->inputBuffer[input]->fifo[switchOutput]->head->Object;
	  scheduleCellStats( SCHEDULE_CELL_STATS_UPDATE_ACCEPT,
			     aSwitch, aCell);

	  if(debug_algorithm)
	    printf("Input %d accepting output %d\n", input, output);
	  return(output);
	}
    }

  if(debug_algorithm)
    printf("Input %d not accepting any output\n", input);

  return(NONE);

}


/* Create schedule state variables for aSwitch */
static void
createScheduleState(aSwitch)
  Switch *aSwitch;
{
  SchedulerState *scheduleState;


  int input, output, numFabricOutputs;

  numFabricOutputs = aSwitch->numOutputs*aSwitch->fabric.Xbar_numOutputLines;

  scheduleState = (SchedulerState *) malloc(sizeof(SchedulerState));
  aSwitch->scheduler.schedulingState = scheduleState;


  scheduleState->inputSched = (InputSchedulerState **)
    malloc( aSwitch->numInputs * sizeof(InputSchedulerState *) );
  for(input=0; input<aSwitch->numInputs; input++)
    scheduleState->inputSched[input] = createInScheduler(aSwitch, input);

  scheduleState->outputSched = (OutputSchedulerState **)
    malloc( numFabricOutputs * sizeof(OutputSchedulerState *) );
  for(output=0; output<numFabricOutputs; output++)
    scheduleState->outputSched[output]=createOutScheduler(aSwitch, output);

}

static InputSchedulerState *
createInScheduler(aSwitch, input)
  Switch *aSwitch;
int input;
{
  SchedulerState 		*scheduleState=aSwitch->scheduler.schedulingState;
  InputSchedulerState	**inputSchedule=scheduleState->inputSched;
  int i, numFabricOutputs;

  numFabricOutputs = aSwitch->numOutputs*aSwitch->fabric.Xbar_numOutputLines;

  inputSchedule[input] = (InputSchedulerState *)
    malloc( sizeof( InputSchedulerState ) );
  inputSchedule[input]->grant = (int *) malloc( sizeof(int)*numFabricOutputs);
  inputSchedule[input]->RR_accepts =
    (int *) malloc( sizeof(int)*numFabricOutputs);
  for(i=0; i<numFabricOutputs; i++ )
    inputSchedule[input]->RR_accepts[i] = i;
  inputSchedule[input]->last_accept=0;

  inputSchedule[input]->RR_requests =
    (int *) malloc( sizeof(int)*numFabricOutputs);
  for(i=0; i<numFabricOutputs; i++ )
    inputSchedule[input]->RR_requests[i] = i;
  inputSchedule[input]->last_request=0;

  return(inputSchedule[input]);

}

static OutputSchedulerState *
createOutScheduler(aSwitch, output)
  Switch *aSwitch;
int output;
{
  SchedulerState 			*scheduleState=aSwitch->scheduler.schedulingState;
  OutputSchedulerState 	**outputSchedule=scheduleState->outputSched;
  int i;

  outputSchedule[output] = (OutputSchedulerState *) malloc( sizeof( OutputSchedulerState ) );
  outputSchedule[output]->request = (int *) malloc( sizeof(int)*aSwitch->numInputs);

  outputSchedule[output]->RR_grants = (int *) malloc( sizeof(int)*aSwitch->numInputs);
  for(i=0; i<aSwitch->numInputs; i++ )
    outputSchedule[output]->RR_grants[i] = i;
  outputSchedule[output]->last_accepted_grant=0;



  return(outputSchedule[output]);
}

#ifdef USE_ALL_FUNCS
static void
printState(aSwitch, aState)
  Switch *aSwitch;
State *aState;
{
  int input, output, numFabricOutputs;

  numFabricOutputs = aSwitch->numOutputs*aSwitch->fabric.Xbar_numOutputLines;

  printf("Switch %2d:", aSwitch->switchNumber);
  for(output=0; output<numFabricOutputs; output++)
    printf("%3d ", output);
  printf("   Output: ");
  for(output=0; output<numFabricOutputs; output++)
    printf("%2d ", output);
  printf("Input: ");
  for(input=0; input<aSwitch->numInputs; input++)
    printf("%2d ", input);
  printf("\n          ");
  /* Connection matrix */
  for(output=0; output<numFabricOutputs; output++)
    {
      if( aState->connection[output] == NONE )
	printf("  X ");
      else
	printf("%3d ", aState->connection[output] );
    }
  printf("           ");
  /* Output schedulers */
  for(output=0; output<numFabricOutputs; output++)
    printf("%2d ", aState->outputGrantPointer[output]);
  printf("       ");
  /* Input schedulers */
  for(input=0; input<aSwitch->numInputs; input++)
    printf("%2d ", aState->inputAcceptPointer[input]);
  printf("\n");
  printf("-----------------------------------------------------------\n");

}
#endif
