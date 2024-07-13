#ifndef CONCURRENTSYSTEM_H
#define CONCURRENTSYSTEM_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <string>
#include <random>
#include <iostream>
#include <optional>

class BoundedBuffer {
public:
    BoundedBuffer(int size) : maxSize(size) {}

    void insert(const std::string& item) {
        std::unique_lock<std::mutex> lock(mutex);
        cond.wait(lock, [this] { return buffer.size() < maxSize; });
        buffer.push(item);
        cond.notify_all();
    }

    std::optional<std::string> tryRemove() {
        std::unique_lock<std::mutex> lock(mutex, std::defer_lock);
        if (!lock.try_lock()) {
            return std::nullopt;
        }

        if (buffer.empty()) {
            return std::nullopt;
        }

        std::string item = buffer.front();
        buffer.pop();
        cond.notify_all();
        return item;
    }

private:
    std::queue<std::string> buffer;
    int maxSize;
    std::mutex mutex;
    std::condition_variable cond;
};

class Producer {
public:
    Producer(int id, int numProducts, BoundedBuffer& queue)
        : id(id), numProducts(numProducts), queue(queue) {}

    void operator()() {
        std::vector<std::string> types = { "SPORTS", "NEWS", "WEATHER" };
        std::default_random_engine generator;
        std::uniform_int_distribution<int> distribution(0, 2);

        for (int i = 0; i < numProducts; ++i) {
            std::string type = types[distribution(generator)];
            std::string product = "Producer " + std::to_string(id) + " " + type + " " + std::to_string(i);
            queue.insert(product);
        }
        queue.insert("DONE");
    }

private:
    int id;
    int numProducts;
    BoundedBuffer& queue;
};

class Dispatcher {
public:
    Dispatcher(std::vector<BoundedBuffer*>& producerQueues,
               BoundedBuffer& sportsQueue,
               BoundedBuffer& newsQueue,
               BoundedBuffer& weatherQueue)
        : producerQueues(producerQueues),
          sportsQueue(sportsQueue),
          newsQueue(newsQueue),
          weatherQueue(weatherQueue),
          doneCount(0),
          totalProducers(producerQueues.size()) {}

    void operator()() {
        size_t index = 0;

        while (doneCount < totalProducers) {
            std::optional<std::string> messageOpt = producerQueues[index]->tryRemove();

            if (messageOpt) {
                std::string message = *messageOpt;
                if (message == "DONE") {
                    ++doneCount;
                } else {
                    if (message.find("SPORTS") != std::string::npos) {
                        sportsQueue.insert(message);
                    } else if (message.find("NEWS") != std::string::npos) {
                        newsQueue.insert(message);
                    } else if (message.find("WEATHER") != std::string::npos) {
                        weatherQueue.insert(message);
                    }
                }
            }

            index = (index + 1) % totalProducers;
        }

        sportsQueue.insert("DONE");
        newsQueue.insert("DONE");
        weatherQueue.insert("DONE");
    }

private:
    std::vector<BoundedBuffer*>& producerQueues;
    BoundedBuffer& sportsQueue;
    BoundedBuffer& newsQueue;
    BoundedBuffer& weatherQueue;
    size_t doneCount;
    size_t totalProducers;
};

class CoEditor {
public:
    CoEditor(BoundedBuffer& inputQueue, BoundedBuffer& outputQueue)
        : inputQueue(inputQueue), outputQueue(outputQueue) {}

    void operator()() {
        while (true) {
            std::string message = inputQueue.tryRemove().value_or("");
            if (message == "DONE") {
                outputQueue.insert("DONE");
                break;
            }
            if (!message.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                outputQueue.insert(message);
            }
        }
    }

private:
    BoundedBuffer& inputQueue;
    BoundedBuffer& outputQueue;
};

class ScreenManager {
public:
    ScreenManager(BoundedBuffer& queue) : queue(queue), doneCount(0) {}

    void operator()() {
        while (doneCount < 3) {
            std::string message = queue.tryRemove().value_or("");
            if (message == "DONE") {
                ++doneCount;
            } else if (!message.empty()) {
                std::cout << message << std::endl;
            }
        }
        std::cout << "DONE" << std::endl;
    }

private:
    BoundedBuffer& queue;
    size_t doneCount;
};

#endif // CONCURRENTSYSTEM_H
