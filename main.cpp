#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <pthread.h>

#include <iostream>
#include <time.h>

using namespace std;

void* run_customer(void* customer_p);
void* run_machine(void* machine_p);

// This struct is used to pass data as orders from customers to machines.
struct order {
	int id; // Customer id that sent the order
	int company_num;
	int amount;
	// Machine id is not included because index in the current_orders list already gives it
};

// This struct holds all of the data about the customer. This struct will be given to a customer thread
struct customer {
	int id;
	int sleep_time;
	int machine_id;
	int company_num;
	int amount;
};

// This struct holds all of the data about the machine. This struct will be given to a machine thread
struct machine {
	int id;
};

// This struct holds the needed data for the output file printing.
struct result {
	int machine_id;
	int customer_id;
	int amount;
	int company_num;
};

// These locks prevent multiple customers reaching to the same machine 
// Each index in this list represents a machine
pthread_mutex_t machine_locks[10];
// These locks prevent multiple machines writing to the same balance_list index
// Each index in this list represents a company
pthread_mutex_t balance_locks[5];
// This lock prevents multiple machines writing to the results at the same time
pthread_mutex_t result_lock;
// This barrier ensures that the customer threads will start to sleep at the same time
pthread_barrier_t barrier;

// Current_orders is the list stores orders given to machines
// Each machine can handle one order per time so the list stores a index for each machine
// Machine indexes goes from 1 to 10 but the list indexes goes from 0 to 9
// So current_order[i-1] corresponds to machine [i]
struct order current_orders[10];
// This result list gathers output file data from machines
// Then it is used in the main thread to print to the output file
// results list is filled whenever a prepayment is done and is locked while writing continues
struct result results[300];

// Balances of each company
int balance_list[5];
// Current size of the results list
int result_size = 0;

int main(int argc, char* argv[]) {

	// Taking the first argument and start reading it
	char* input_file = argv[1];
	FILE* input_fptr = fopen(input_file, "r");

	char line[256];
	// Taking customer count from the first line
	fgets(line, 256, input_fptr); 
	int customer_num = atoi(line);
	struct customer customers[customer_num];
	for (int i = 0; i < customer_num; i++) {
		//Get data line by line with fgets
		fgets(line, 256, input_fptr);
		
		//In this section line is parsed, readed, converted to int and stored in the customer list
		char* p[4];
		p[0] = &(line[0]);
		int current_p = 1;
		int j = 0;
		while (line[j] != 10) {
			if (line[j] == 44) {
				p[current_p]= &(line[j + 1]);
				line[j] = 0;
				current_p++;
			}
			j++;
		}
		customers[i].id = i+1;
		customers[i].sleep_time = atoi(p[0]);
		customers[i].machine_id = atoi(p[1]);
		// Company names are converted to a company num
		if (strcmp("Kevin", p[2]) == 0) {
			customers[i].company_num = 0;
		}
		if (strcmp("Bob", p[2]) == 0) {
			customers[i].company_num = 1;
		}
		if (strcmp("Stuart", p[2]) == 0) {
			customers[i].company_num = 2;
		}
		if (strcmp("Otto", p[2]) == 0) {
			customers[i].company_num = 3;
		}
		if (strcmp("Dave", p[2]) == 0) {
			customers[i].company_num = 4;
		}
		customers[i].amount = atoi(p[3]);
	}
	
	//Initializing mutex locks and the barrier
	for (int i = 0; i < 10; i++) {
		pthread_mutex_init(&(machine_locks[i]), NULL);
	}
	
	for (int i = 0; i < 5; i++) {
		pthread_mutex_init(&(balance_locks[i]), NULL);
	}
	
	pthread_mutex_init(&result_lock, NULL);

	pthread_barrier_init(&barrier, NULL, customer_num);

	// Firstly machine threads are created, each given a machine struct converted to a void pointer
	struct machine machines[10];
	pthread_t machines_tid_list[10];
	pthread_attr_t machines_attr_list[10];
	for (int i = 0; i < 10; i++) {
		machines[i].id = i+1;
		pthread_attr_init(&(machines_attr_list[i]));
		pthread_create(&(machines_tid_list[i]), &(machines_attr_list[i]), run_machine, (void*)&machines[i]);
	}

	// Then customer threads are created, each given a customer struct converted to a void pointer
	pthread_t tid_list[customer_num];
	pthread_attr_t attr_list[customer_num];
	for (int i = 0; i < customer_num; i++) {
		pthread_attr_init(&(attr_list[i]));
		pthread_create(&(tid_list[i]), &(attr_list[i]), run_customer ,(void*)&customers[i]);
	}

	// Now main thread waits for the customer threads to be done
	// Main does not wait for the machine threads because they are in a infinite loop
	for (int i = 0; i < customer_num; i++) {
		pthread_join(tid_list[i], NULL);
	}
	
	// Once all customer threads are joined, we can start printing
	// Firstly output_file  is created. It is converted to char* to use in the fopen method
	string output_file_str(input_file);
	output_file_str.insert(output_file_str.length()-4, "_log");

	char* output_file = new char[output_file_str.length() + 1];

	strcpy(output_file, output_file_str.c_str());

	FILE* output_fptr = fopen(output_file, "w");
	// After the output file is created, printing out starts 
	for (int i = 0; i < customer_num; i++) {
		// Company_num is converted to char list again
		char company_name[7];
		if (results[i].company_num == 0) {
			strcpy(company_name, "Kevin");
		}
		if (results[i].company_num == 1) {
			strcpy(company_name, "Bob");
		}
		if (results[i].company_num == 2) {
			strcpy(company_name, "Stuart");
		}
		if (results[i].company_num == 3) {
			strcpy(company_name, "Otto");
		}
		if (results[i].company_num == 4) {
			strcpy(company_name, "Dave");
		}
		fprintf(output_fptr,"Customer%d,%dTL,%s\n", results[i].customer_id, results[i].amount, company_name);
	}
	fprintf(output_fptr, "All prepayments are completed.\n");
	fprintf(output_fptr, "Kevin: %d\n", balance_list[0]);
	fprintf(output_fptr, "Bob: %d\n", balance_list[1]);
	fprintf(output_fptr, "Stuart: %d\n", balance_list[2]);
	fprintf(output_fptr, "Otto: %d\n", balance_list[3]);
	fprintf(output_fptr, "Dave: %d", balance_list[4]);


	pthread_barrier_destroy(&barrier);

	return 0;
}

void* run_customer(void* customer_p) {
	// Firstly read the data from the customer struct given as a void pointer
	struct customer* customer = (struct customer*)customer_p;
	int id = customer->id;
	int sleep_time = customer->sleep_time;
	int machine_id = customer->machine_id;
	int company_num = customer->company_num;
	int amount = customer->amount;

	// All customer threads are brought here and they start sleeping at the same time
	pthread_barrier_wait(&barrier);
	
	struct timespec sleep_time2 = { 0, sleep_time*1000000 };
	nanosleep(&sleep_time2,NULL);

	// When the customer awakes, it locks a machine lock 
	pthread_mutex_lock(&(machine_locks[machine_id-1]));
	
	// Current_orders is the list stores orders given to machines.
	// Each machine can handle one order per time so the list stores a index for each machine
	// Here the customer fills the order for the wanted machine_id
	// The machine checks the id to start working on the order so the id is filled last
	current_orders[machine_id-1].company_num = company_num;
	current_orders[machine_id-1].amount = amount;
	current_orders[machine_id-1].id = id;
	
	// Here customer waits for the machine to set its customer_id to 0 in the current_orders
	// Machine thread sets the customer_id to 0 when it's done with the order
	while (current_orders[machine_id-1].id == id) {
		continue;
	}
	// If the machine is done with the order then customer can release the machine by releasing its lock
	pthread_mutex_unlock(&(machine_locks[machine_id-1]));
	
	pthread_exit(NULL);
}

void* run_machine(void* machine_p) {
	// Firstly read the data from the machine struct given as a void pointer
	struct machine* machine = (struct machine*)machine_p;
	int id = machine->id;
	while (true) {
		// Machine waits for customer id to be changed by a customer in the current_orders
		while (current_orders[id-1].id == 0) {
			continue;
		}
		// Read the order from the current_orders 
		int customer_id = current_orders[id-1].id;
		int company_num = current_orders[id-1].company_num;
		int order_amount = current_orders[id-1].amount;

		// Then lock the balance of the corresponding company then add the order amount
		pthread_mutex_lock(&(balance_locks[company_num]));
		balance_list[company_num] += order_amount;
		pthread_mutex_unlock(&(balance_locks[company_num]));

		
		// Later lock the results list and add the this finished prepayment
		pthread_mutex_lock(&result_lock);
		
		results[result_size].customer_id = customer_id;
		results[result_size].machine_id = id;
		results[result_size].amount = order_amount;
		results[result_size].company_num = company_num;
		result_size++;
		
		pthread_mutex_unlock(&result_lock);

		// This tells the customer owning this order that machine is done with this order
		current_orders[id-1].id = 0;
		
		// Machine continuously waits for orders so the machine returns to the beginning until main thread finishes
	}
}