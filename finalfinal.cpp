#include "system.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <thread>
#include <iostream>

int main(int argc, char* argv[]) 
{
    // Check if the number of arguments is correct
    if (argc != 2) 
    {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }

    // Open the configuration file
    std::ifstream configFile(argv[1]);
    if (!configFile) 
    {
        std::cerr << "Error opening config file." << std::endl;
        return 1;
    }

    // Vectors to hold producers, threads, and bounded buffers
    std::vector<Producer> producerList;
    std::vector<std::thread> producerThreads;
    std::vector<BoundedBuffer*> producerBuffers;
    int producerCount = 0;

    std::string line;
    // Read the configuration file line by line
    while (std::getline(configFile, line))
    {
        // Check for the "PRODUCER" keyword
        if (line.find("PRODUCER") != std::string::npos) 
        {
            int id = ++producerCount;
            int productCount;
            int bufferSize;

            // Read the number of products
            std::getline(configFile, line);
            std::istringstream(line) >> productCount;
            
            // Read the queue size
            std::getline(configFile, line);
            std::istringstream(line.substr(line.find("=") + 1)) >> bufferSize;

            // Create a bounded buffer for the producer
            BoundedBuffer* buffer = new BoundedBuffer(bufferSize);
            producerBuffers.push_back(buffer);
            producerList.emplace_back(id, productCount, *buffer);
        }
    }

    int coEditorBufferSize;

    // Read the queue size for the co-editors
    std::getline(configFile, line);
    std::getline(configFile, line);
    std::istringstream(line.substr(line.find("=") + 1)) >> coEditorBufferSize;

    // Create bounded buffers for sports, news, weather, and co-editor
    BoundedBuffer sportsBuffer(coEditorBufferSize);
    BoundedBuffer newsBuffer(coEditorBufferSize);
    BoundedBuffer weatherBuffer(coEditorBufferSize);
    BoundedBuffer coEditorBuffer(coEditorBufferSize);

    // Initialize dispatcher and co-editors
    Dispatcher dispatcher(producerBuffers, sportsBuffer, newsBuffer, weatherBuffer);
    CoEditor sportsCoEditor(sportsBuffer, coEditorBuffer);
    CoEditor newsCoEditor(newsBuffer, coEditorBuffer);
    CoEditor weatherCoEditor(weatherBuffer, coEditorBuffer);
    ScreenManager screenManager(coEditorBuffer);

    // Create threads for dispatcher, co-editors, and screen manager
    std::thread dispatcherThread(dispatcher);
    std::thread sportsCoEditorThread(sportsCoEditor);
    std::thread newsCoEditorThread(newsCoEditor);
    std::thread weatherCoEditorThread(weatherCoEditor);
    std::thread screenManagerThread(screenManager);

    // Create threads for each producer
    for (auto& producer : producerList) 
    {
        producerThreads.emplace_back(producer);
    }

    // Join all producer threads
    for (auto& thread : producerThreads) 
    {
        thread.join();
    }

    // Join dispatcher and co-editor threads
    dispatcherThread.join();
    sportsCoEditorThread.join();
    newsCoEditorThread.join();
    weatherCoEditorThread.join();
    screenManagerThread.join();

    // Clean up dynamically allocated buffers
    for (auto buffer : producerBuffers) 
    {
        delete buffer;
    }

    return 0;
}
