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

class BoundedBuffer
{
public:
    using size_type = std::queue<std::string>::size_type;

    BoundedBuffer(size_type amount) : maxAmount(amount) {}

    // Insert new item to the buffer, if the buffer is full - wait when it will be place
    void insert(const std::string& item)
    {
        std::unique_lock<std::mutex> lock(mutex);
        cond_var.wait(lock, [this] { return buffer.size() < maxAmount; });
        buffer.push(item);
        cond_var.notify_all();
    }

    // Remove item from the buffer and return it, if the buffer is empty, wait for item
    std::string remove()
    {
        std::unique_lock<std::mutex> lock(mutex);
        cond_var.wait(lock, [this] { return !buffer.empty(); });
        std::string item = buffer.front();
        buffer.pop();
        cond_var.notify_all();
        return item;
    }

    // Check if we can remove from the buffer (not empty)
    bool tryRemove(std::string& item)
    {
        std::unique_lock<std::mutex> lock(mutex);
        if (buffer.empty())
        {
            return false;
        }
        item = buffer.front();
        buffer.pop();
        cond_var.notify_all();
        return true;
    }

private:
    std::queue<std::string> buffer;
    size_type maxAmount;
    std::mutex mutex;
    std::condition_variable cond_var;
};

class Producer
{
public:
    Producer(int id, int numProducts, BoundedBuffer& queue)
        : id(id), numProducts(numProducts), queue(queue) {}

    void operator()()
    {
        std::vector<std::string> categories = { "SPORTS", "NEWS", "WEATHER" };
        std::default_random_engine rander;
        std::uniform_int_distribution<int> numbers(0, 2);

        for (int i = 0; i < numProducts; ++i)
        {
            std::string category = categories[numbers(rander)];
            std::string message;

            if (category == "SPORTS")
            {
                message = "Producer " + std::to_string(id) + " " + category + " " + std::to_string(category_Counter.sport);
                category_Counter.sport++;
            }
            else if (category == "NEWS")
            {
                message = "Producer " + std::to_string(id) + " " + category + " " + std::to_string(category_Counter.news);
                category_Counter.news++;
            }
            else if (category == "WEATHER")
            {
                message = "Producer " + std::to_string(id) + " " + category + " " + std::to_string(category_Counter.weather);
                category_Counter.weather++;
            }
            queue.insert(message);
        }
        queue.insert("DONE");
    }

private:
    struct Category_Counter
    {
        int sport = 0;
        int news = 0;
        int weather = 0;
    } category_Counter;

    int id;
    int numProducts;
    BoundedBuffer& queue;
};

class Dispatcher
{
public:
    Dispatcher(std::vector<BoundedBuffer*>& producer_buffers, BoundedBuffer& sports_buffer, BoundedBuffer& news_buffer, BoundedBuffer& weather_buffer)
        : producer_buffers(producer_buffers), sports_buffer(sports_buffer), news_buffer(news_buffer), weather_buffer(weather_buffer), doneCount(0) {}

    void operator()() {
        size_t producers_number = producer_buffers.size();
        size_t index = 0;
        while (doneCount < producers_number)
        {
            std::string message;
            if (producer_buffers[index]->tryRemove(message))
            {
                if (message == "DONE")
                {
                    ++doneCount;
                }
                else
                {
                    if (message.find("SPORTS") != std::string::npos) {
                        sports_buffer.insert(message);
                    }
                    else if (message.find("NEWS") != std::string::npos) {
                        news_buffer.insert(message);
                    }
                    else if (message.find("WEATHER") != std::string::npos) {
                        weather_buffer.insert(message);
                    }
                }
            }
            index = (index + 1) % producers_number;
        }
        sports_buffer.insert("DONE");
        news_buffer.insert("DONE");
        weather_buffer.insert("DONE");
    }

private:
    std::vector<BoundedBuffer*>& producer_buffers;
    BoundedBuffer& sports_buffer;
    BoundedBuffer& news_buffer;
    BoundedBuffer& weather_buffer;
    size_t doneCount;
};

class CoEditor {
public:
    CoEditor(BoundedBuffer& input_buffer, BoundedBuffer& output_buffer)
        : input_buffer(input_buffer), output_buffer(output_buffer) {}
    void operator()()
    {
        while (true)
        {
            std::string message = input_buffer.remove();
            if (message == "DONE")
            {
                output_buffer.insert("DONE");
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            output_buffer.insert(message);
        }
    }
private:
    BoundedBuffer& input_buffer;
    BoundedBuffer& output_buffer;
};

class ScreenManager
{
public:
    ScreenManager(BoundedBuffer& buffer) : buffer(buffer), counter(0) {}
    void operator()()
    {
        while (counter < 3)
        {
            std::string message = buffer.remove();
            if (message == "DONE")
            {
                ++counter;
            }
            else
            {
                std::cout << message << std::endl;
            }
        }
        std::cout << "DONE" << std::endl;
    }

private:
    BoundedBuffer& buffer;
    size_t counter;
};

#endif
