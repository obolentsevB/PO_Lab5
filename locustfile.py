from locust import HttpUser, task, between


class WebsiteUser(HttpUser):
    wait_time = between(1, 3)

    @task(2)
    def open_index(self):
        self.client.get("/")

    @task(1)
    def open_page2(self):
        self.client.get("/page2.html")
