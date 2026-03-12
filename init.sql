CREATE TABLE IF NOT EXISTS contacts (
    id SERIAL PRIMARY KEY,
    last_name VARCHAR(100) NOT NULL,
    first_name VARCHAR(100) NOT NULL,
    patronymic VARCHAR(100),
    phone_number VARCHAR(20) NOT NULL,
    note TEXT
);

INSERT INTO contacts (last_name, first_name, patronymic, phone_number, note) VALUES
    ('Иванов', 'Иван', 'Иванович', '+7-900-123-45-67', 'Друг из университета'),
    ('Петров', 'Петр', 'Петрович', '+7-901-234-56-78', 'Коллега по работе'),
    ('Сидоров', 'Сидор', 'Сидорович', '+7-902-345-67-89', 'Начальник'),
    ('Кузнецов', 'Александр', 'Владимирович', '+7-903-456-78-90', 'Врач'),
    ('Смирнов', 'Дмитрий', '-', '+7-904-567-89-01', 'Таксист');
