#include <iostream>
#include <cstring>
#include <thread>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/prctl.h>
#include <ctime>
#include <mutex>
#include <condition_variable>
#include <csignal>

using namespace std;
mutex mutex_m;
condition_variable cv;
#define SHM_SIZE 1024

char* shared_message;

// Function to stop the process clear shared memory and exit program
void cleanup_and_exit(int signal)
{
    cout << "Received signal " << signal << endl<<". Cleaning up..." << endl;
    // Detaches the shared memory
    if (shmdt(shared_message) == -1)
    {
        perror("shmdt");
        exit(EXIT_FAILURE);
    }
    // Removing the shared memory
    key_t key = ftok("/home/jayanth-1090/Desktop/sharedd.cpp", 'R');
    int shm_id = shmget(key, SHM_SIZE, 0666);
    if (shm_id != -1)
    {
        if (shmctl(shm_id, IPC_RMID, nullptr) == -1)
        {
            perror("shmctl");
            exit(EXIT_FAILURE);
        }
    }
    exit(EXIT_SUCCESS);
}

void* Write_f(void* args)
{
    char* shared_message = static_cast<char*>(args);
    for (int i = 1; i <= 1000; ++i)
    {
        {
            std::lock_guard<std::mutex> lg(mutex_m);
            cout << "W" << i << endl;
            sprintf(shared_message, "%s%d", shared_message, i);
            cv.notify_one();
        }
        sleep(1);
    }
    return nullptr;
}

void* Read_f(void* args)
{
    char* shared_message = static_cast<char*>(args);
    for (int i = 1; i <= 1000; ++i)
    {
        {
            std::unique_lock<std::mutex> ul(mutex_m);
            cv.wait(ul, [&shared_message, i] {
                cout << "R" << shared_message << endl;
                return shared_message[strlen(shared_message)] == i;
            });
        }
        sleep(1);
    }
    return nullptr;
}

void create_shared()
{
    // Generating a key such that the process that has this key can access the shared memory
    key_t key = ftok("/home/jayanth-1090/Desktop/sharedd.cpp", 'R');
    int shm_id = shmget(key, SHM_SIZE, IPC_CREAT | 0666);
    if (shm_id == -1)
    {
        perror("shmget");
        exit(EXIT_FAILURE);
    }
    // Attaching the shared memory to the allocated ID
    shared_message = static_cast<char*>(shmat(shm_id, nullptr, 0));
    if (shared_message == reinterpret_cast<void*>(-1))
    {
        perror("shmat");
        exit(EXIT_FAILURE);
    }
}

int main()
{
    pid_t pid;
    pthread_t tid1, tid2;
    cout << getppid() << endl;
    prctl(PR_SET_NAME, (unsigned long)"Hi_iTs_mE");
    create_shared();
    // Register the signal handler for cleaning up
    signal(SIGINT, cleanup_and_exit);
    // Creating threads to write and read data
    if (pthread_create(&tid1, nullptr, Write_f, shared_message) != 0)
    {
        perror("pthread_create");
        cerr << "Error creating the thread" << endl;
        exit(EXIT_FAILURE);
    }
    if (pthread_create(&tid2, nullptr, Read_f, shared_message) != 0)
    {
        perror("pthread_create");
        cerr << "Error creating the thread" << endl;
        exit(EXIT_FAILURE);
    }
    // Joining the threads
    if (pthread_join(tid1, nullptr) != 0)
    {
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }
    if (pthread_join(tid2, nullptr) != 0)
    {
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }
    cleanup_and_exit(0); // Clean up before exiting
    return 0;
}

