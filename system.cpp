#include "system.h"
#include <fstream>
#include <sstream>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }

    std::ifstream configFile(argv[1]);
    if (!configFile) {
        std::cerr << "Error opening config file." << std::endl;
        return 1;
    }

    std::vector<Producer> producers;
    std::vector<std::thread> producerThreads;
    std::vector<BoundedBuffer*> producerQueues;
    int numProducers = 0;

    std::string line;
    while (std::getline(configFile, line)) {
        if (line.find("PRODUCER") != std::string::npos) {
            int id = ++numProducers;
            int numProducts;
            int queueSize;

            std::getline(configFile, line);
            std::istringstream(line) >> numProducts;
            std::getline(configFile, line);
            std::istringstream(line.substr(line.find("=") + 1)) >> queueSize;

            BoundedBuffer* queue = new BoundedBuffer(queueSize);
            producerQueues.push_back(queue);
            producers.emplace_back(id, numProducts, *queue);
        }
    }

    int coEditorQueueSize;
    std::getline(configFile, line);
    std::getline(configFile, line);
    std::istringstream(line.substr(line.find("=") + 1)) >> coEditorQueueSize;

    BoundedBuffer sportsQueue(coEditorQueueSize);
    BoundedBuffer newsQueue(coEditorQueueSize);
    BoundedBuffer weatherQueue(coEditorQueueSize);
    BoundedBuffer coEditorQueue(coEditorQueueSize);

    Dispatcher dispatcher(producerQueues, sportsQueue, newsQueue, weatherQueue);
    CoEditor sportsCoEditor(sportsQueue, coEditorQueue);
    CoEditor newsCoEditor(newsQueue, coEditorQueue);
    CoEditor weatherCoEditor(weatherQueue, coEditorQueue);
    ScreenManager screenManager(coEditorQueue);

    std::thread dispatcherThread(dispatcher);
    std::thread sportsCoEditorThread(sportsCoEditor);
    std::thread newsCoEditorThread(newsCoEditor);
    std::thread weatherCoEditorThread(weatherCoEditor);
    std::thread screenManagerThread(screenManager);

    for (auto& producer : producers) {
        producerThreads.emplace_back(producer);
    }

    for (auto& thread : producerThreads) {
        thread.join();
    }
    dispatcherThread.join();
    sportsCoEditorThread.join();
    newsCoEditorThread.join();
    weatherCoEditorThread.join();
    screenManagerThread.join();

    for (auto queue : producerQueues) {
        delete queue;
    }

    return 0;
}
